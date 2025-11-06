// MiniGridMonitor - device condition capture UI
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <chrono>
#include <random>
#include <future>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <vector>
#include <regex>
#include <cmath>
#include <map>
#include <string>
#include <algorithm>

// Debug logging function
static void log_debug(const std::string& msg) {
    std::ofstream log("debug.log", std::ios::app);
    if (log.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string timestamp = std::ctime(&time);
        if (!timestamp.empty() && timestamp[timestamp.length()-1] == '\n') {
            timestamp.erase(timestamp.length()-1);
        }
        log << "[" << timestamp << "] " << msg << std::endl;
    }
}

// Validation helper class
class FormValidator {
public:
    struct ValidationResult {
        bool isValid;
        std::string message;
        
        ValidationResult(bool valid = true, const std::string& msg = "")
            : isValid(valid), message(msg) {}
        
        operator bool() const { return isValid; }
    };

    static ValidationResult required(const std::string& value, const std::string& fieldName) {
        if (value.empty()) {
            return {false, fieldName + " is required"};
        }
        return {true};
    }

    static ValidationResult lengthRange(const std::string& value, size_t minLen, size_t maxLen, const std::string& fieldName) {
        if (!value.empty() && (value.length() < minLen || value.length() > maxLen)) {
            return {false, fieldName + " must be between " + std::to_string(minLen) + " and " + std::to_string(maxLen) + " characters"};
        }
        return {true};
    }

    static ValidationResult regex(const std::string& value, const std::string& pattern, const std::string& fieldName) {
        if (!value.empty()) {
            std::regex re(pattern);
            if (!std::regex_match(value, re)) {
                return {false, fieldName + " contains invalid characters"};
            }
        }
        return {true};
    }

    static ValidationResult floatRange(const std::string& value, float min, float max, const std::string& fieldName) {
        if (!value.empty()) {
            try {
                float val = std::stof(value);
                if (val < min || val > max) {
                    return {false, fieldName + " must be between " + std::to_string(min) + " and " + std::to_string(max)};
                }
            } catch (...) {
                return {false, fieldName + " must be a valid number"};
            }
        }
        return {true};
    }

    static ValidationResult intRange(const std::string& value, int min, int max, const std::string& fieldName) {
        if (!value.empty()) {
            try {
                int val = std::stoi(value);
                if (val < min || val > max) {
                    return {false, fieldName + " must be between " + std::to_string(min) + " and " + std::to_string(max)};
                }
            } catch (...) {
                return {false, fieldName + " must be a valid integer"};
            }
        }
        return {true};
    }

    static ValidationResult enumValue(const std::string& value, const std::vector<std::string>& allowedValues, const std::string& fieldName) {
        if (!value.empty()) {
            if (std::find(allowedValues.begin(), allowedValues.end(), value) == allowedValues.end()) {
                return {false, fieldName + " has an invalid value"};
            }
        }
        return {true};
    }
};

static std::string generate_uuid_v4()
{
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist32(0, 0xffffffffu);

    auto d = [&]() { return dist32(gen); };
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::nouppercase;
    ss << std::setw(8) << (d());
    ss << "-" << std::setw(4) << ((d() >> 16) & 0xffff);
    ss << "-" << std::setw(4) << ((d() >> 16) & 0xffff);
    ss << "-" << std::setw(4) << ((d() >> 16) & 0xffff);
    ss << "-" << std::setw(12) << ((uint64_t)d() << 32 | d());
    return ss.str();
}

static std::string current_timestamp()
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

static std::string json_escape(const std::string& s)
{
    std::ostringstream o;
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"': o << "\\\""; break;
        case '\\': o << "\\\\"; break;
        case '\b': o << "\\b"; break;
        case '\f': o << "\\f"; break;
        case '\n': o << "\\n"; break;
        case '\r': o << "\\r"; break;
        case '\t': o << "\\t"; break;
        default:
            if (c < 0x20)
            {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec << std::setfill(' ');
            }
            else
            {
                o << c;
            }
        }
    }
    return o.str();
}

static std::string csv_escape(const std::string& s)
{
    bool needQuotes = false;
    for (unsigned char c : s)
    {
        if (c == '"' || c == ',' || c == '\n' || c == '\r') { needQuotes = true; break; }
    }
    std::string out;
    if (!needQuotes) return s;
    out.push_back('"');
    for (unsigned char c : s)
    {
        if (c == '"') out.append("""");
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static std::vector<std::string> parse_csv_line(const std::string& line)
{
    std::vector<std::string> cols;
    std::string cur;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i)
    {
        char c = line[i];
        if (inQuotes)
        {
            if (c == '"')
            {
                if (i + 1 < line.size() && line[i + 1] == '"')
                {
                    cur.push_back('"');
                    ++i;
                }
                else
                {
                    inQuotes = false;
                }
            }
            else
            {
                cur.push_back(c);
            }
        }
        else
        {
            if (c == '"')
            {
                inQuotes = true;
            }
            else if (c == ',')
            {
                cols.push_back(cur);
                cur.clear();
            }
            else
            {
                cur.push_back(c);
            }
        }
    }
    cols.push_back(cur);
    return cols;
}

static std::string get_appdata_devices_path()
{
    // Use fixed project folder per user request
    namespace fs = std::filesystem;
    fs::path dir = R"(V:\PersonalCodeBase\MiniGridMonitor)";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return (dir / "devices.csv").string();
}
// Save CSV row into `path`. Creates file with header if missing. Appends rows.
static void save_device_csv(const std::string& path, const std::string& csv_row)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    bool exists = fs::exists(path, ec);
    // Ensure directory exists (should be already)
    fs::path p(path);
    fs::path dir = p.parent_path();
    if (!dir.empty()) fs::create_directories(dir, ec);

    std::ofstream out(path, std::ios::out | std::ios::app | std::ios::binary);
    if (!out)
        throw std::runtime_error("unable to open file for append: " + path);

    if (!exists)
    {
        // write header
        out << "uuid,created_at,operator_id,instance_id,app_version,device_id,device_name,status,action_type,voltage,temperature,severity,ui_latency_ms,notes\n";
    }
    out << csv_row << "\n";
    out.close();
}

class MyFrame : public wxFrame
{
public:
    MyFrame()
        : wxFrame(nullptr, wxID_ANY, "Device Condition Monitor")
    {
        try {
            log_debug("MyFrame constructor starting");
            SetMinSize(wxSize(800, 600));
            Centre();
        wxPanel* panel = new wxPanel(this);
        wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);

        // Create the header panel with dark blue background
        wxPanel* headerPanel = new wxPanel(panel, wxID_ANY);
        headerPanel->SetBackgroundColour(wxColour(0x00, 0x33, 0x66)); // #003366
        wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);

        // Create the heading text
        wxStaticText* heading = new wxStaticText(headerPanel, wxID_ANY, "Device Condition Monitor");
        heading->SetForegroundColour(wxColour(255, 255, 255));
        wxFont headingFont(13, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        headingFont.SetFaceName("Segoe UI");
        heading->SetFont(headingFont);

        // Add heading to header sizer with centering
        headerSizer->AddStretchSpacer();
        headerSizer->Add(heading, 0, wxALIGN_CENTER_VERTICAL);
        headerSizer->AddStretchSpacer();

        // Set padding for the header panel
        headerPanel->SetSizer(headerSizer);
        headerPanel->SetMinSize(wxSize(-1, 40)); // Set minimum height for padding
        topSizer->Add(headerPanel, 0, wxEXPAND);
        topSizer->AddSpacer(8); // Add padding below header

        // Create a container for all fields with top padding
        wxBoxSizer* fieldsContainer = new wxBoxSizer(wxVERTICAL);
        fieldsContainer->AddSpacer(50); // Add top padding

        // Form grid - 2 columns: label and field
        wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 5);
        grid->AddGrowableCol(1, 1);
        
        // Error text styling
        wxFont errorFont = wxFont(wxNORMAL_FONT->GetPointSize() - 1, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        wxColor errorColor = wxColor(200, 0, 0);

        auto addField = [&](const wxString& label, wxTextCtrl*& field, wxStaticText*& error) {
            // Add label and field
            grid->Add(new wxStaticText(panel, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
            wxBoxSizer* fieldContainer = new wxBoxSizer(wxVERTICAL);
                field = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1));
            fieldContainer->Add(field, 0, wxEXPAND);
            
            // Add error message below
            error = new wxStaticText(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
            error->SetForegroundColour(errorColor);
            error->SetFont(errorFont);
            fieldContainer->Add(error, 0, wxEXPAND | wxTOP, 2);
            
            grid->Add(fieldContainer, 1, wxEXPAND);
        };

        auto addChoice = [&](const wxString& label, wxChoice*& field, wxStaticText*& error, const wxArrayString& choices) {
            // Add label and field
            grid->Add(new wxStaticText(panel, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
            wxBoxSizer* fieldContainer = new wxBoxSizer(wxVERTICAL);
                field = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxSize(300, -1), choices);
            field->SetSelection(0);
            fieldContainer->Add(field, 0, wxEXPAND);
            
            // Add error message below
            error = new wxStaticText(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
            error->SetForegroundColour(errorColor);
            error->SetFont(errorFont);
            fieldContainer->Add(error, 0, wxEXPAND | wxTOP, 2);
            
            grid->Add(fieldContainer, 1, wxEXPAND);
        };

    // Operator ID
    wxBoxSizer* operatorContainer = new wxBoxSizer(wxVERTICAL);
    grid->Add(new wxStaticText(panel, wxID_ANY, "Operator ID:"), 0, wxALIGN_CENTER_VERTICAL);
        wxBoxSizer* fieldContainer = new wxBoxSizer(wxVERTICAL);
        m_operatorId = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1));
        fieldContainer->Add(m_operatorId, 0, wxEXPAND);
        
        m_operatorIdError = new wxStaticText(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
        m_operatorIdError->SetForegroundColour(errorColor);
        m_operatorIdError->SetFont(errorFont);
        fieldContainer->Add(m_operatorIdError, 0, wxEXPAND | wxTOP, 2);
        operatorContainer->Add(fieldContainer, 0, wxEXPAND);
        grid->Add(operatorContainer, 1, wxEXPAND);

        // Instance ID
        addField("Instance ID:", m_instanceId, m_instanceIdError);

        // App Version
        addField("App Version:", m_appVersion, m_appVersionError);
        m_appVersion->SetValue("1.0.0");

        // Device ID
        addField("Device ID:", m_deviceId, m_deviceIdError);

        // Device Name
        addField("Device Name:", m_deviceName, m_deviceNameError);

        // Device Status
        wxArrayString statusChoices;
        statusChoices.Add("Unknown");
        statusChoices.Add("Online");
        statusChoices.Add("Offline");
        statusChoices.Add("Degraded");
        addChoice("Device Status:", m_status, m_statusError, statusChoices);

        // Action Type
        wxArrayString actionChoices;
        actionChoices.Add("Check");
        actionChoices.Add("Maintenance");
        actionChoices.Add("Repair");
        actionChoices.Add("Replace");
        addChoice("Action Type:", m_actionType, m_actionTypeError, actionChoices);

        // Voltage
        addField("Voltage (V):", m_voltage, m_voltageError);

        // Temperature
        addField("Temperature (°C):", m_temperature, m_temperatureError);

        // Severity
        wxArrayString severityChoices;
        severityChoices.Add("Low");
        severityChoices.Add("Medium");
        severityChoices.Add("High");
        severityChoices.Add("Critical");
        addChoice("Severity:", m_severity, m_severityError, severityChoices);

        // UI Latency
        addField("UI Latency (ms):", m_uiLatency, m_uiLatencyError);
        m_uiLatency->SetValue("0");

        // Notes field
        grid->Add(new wxStaticText(panel, wxID_ANY, "Notes:"), 0, wxALIGN_TOP);
        wxBoxSizer* notesContainer = new wxBoxSizer(wxVERTICAL);
        m_notes = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(300, 60), wxTE_MULTILINE);
        notesContainer->Add(m_notes, 0, wxEXPAND);
        
        // Add error message below notes
        m_notesError = new wxStaticText(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
        m_notesError->SetForegroundColour(errorColor);
        m_notesError->SetFont(errorFont);
        notesContainer->Add(m_notesError, 0, wxEXPAND | wxTOP, 2);
        
        grid->Add(notesContainer, 1, wxEXPAND);

        // Create a scrolled window for the form
        wxScrolledWindow* scrolledWindow = new wxScrolledWindow(panel);
        scrolledWindow->SetScrollRate(0, 20); // Vertical scrolling only

        // Create container for centered content
        wxBoxSizer* centeringSizer = new wxBoxSizer(wxHORIZONTAL);
        centeringSizer->AddStretchSpacer(1);
        
        wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);
        contentSizer->SetMinSize(900, -1);  // Set minimum width for the form area
        
        // Add grid to fields container
        fieldsContainer->Add(grid, 0, wxEXPAND | wxALL, 10);

        // Add fields container to content sizer
        contentSizer->Add(fieldsContainer, 0, wxEXPAND);

        // Buttons
        wxBoxSizer* btns = new wxBoxSizer(wxHORIZONTAL);
        
        // Create a panel for the Add Device button with border
        wxPanel* addBtnPanel = new wxPanel(panel, wxID_ANY);
        addBtnPanel->SetBackgroundColour(wxColour(0x00, 0x33, 0x66)); // Dark blue border (#003366)
        wxBoxSizer* addBtnSizer = new wxBoxSizer(wxVERTICAL);
        m_addBtn = new wxButton(addBtnPanel, wxID_ANY, "Add Device", wxDefaultPosition, wxSize(120, 30), wxBORDER_NONE);
        m_addBtn->SetBackgroundColour(wxColour(0x00, 0x33, 0x66)); // Dark blue background (#003366)
        m_addBtn->SetForegroundColour(wxColour(255, 255, 255)); // White text
        m_addBtn->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent&) {
            m_addBtn->SetBackgroundColour(wxColour(255, 255, 255)); // White background on hover
            m_addBtn->SetForegroundColour(wxColour(0x00, 0x33, 0x66)); // Dark blue text on hover (#003366)
            m_addBtn->Refresh();
        });
        m_addBtn->Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
            m_addBtn->SetBackgroundColour(wxColour(0x00, 0x33, 0x66)); // Back to dark blue background (#003366)
            m_addBtn->SetForegroundColour(wxColour(255, 255, 255)); // Back to white text
            m_addBtn->Refresh();
        });
        addBtnSizer->Add(m_addBtn, 1, wxALL, 1); // 1 pixel border
        addBtnPanel->SetSizer(addBtnSizer);

        // Create a panel for the Clear Fields button with border
        wxPanel* clearBtnPanel = new wxPanel(panel, wxID_ANY);
        clearBtnPanel->SetBackgroundColour(wxColour(0x00, 0x33, 0x66)); // Dark blue border (#003366)
        wxBoxSizer* clearBtnSizer = new wxBoxSizer(wxVERTICAL);
        m_clearBtn = new wxButton(clearBtnPanel, wxID_ANY, "Clear Fields", wxDefaultPosition, wxSize(120, 30), wxBORDER_NONE);
        m_clearBtn->SetBackgroundColour(wxColour(255, 255, 255)); // White background
        m_clearBtn->SetForegroundColour(wxColour(0x00, 0x33, 0x66)); // Dark blue text (#003366)
        m_clearBtn->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent&) {
            m_clearBtn->SetBackgroundColour(wxColour(0x00, 0x33, 0x66)); // Dark blue background on hover (#003366)
            m_clearBtn->SetForegroundColour(wxColour(255, 255, 255)); // White text on hover
            m_clearBtn->Refresh();
        });
        m_clearBtn->Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) {
            m_clearBtn->SetBackgroundColour(wxColour(255, 255, 255)); // Back to white background
            m_clearBtn->SetForegroundColour(wxColour(0x00, 0x33, 0x66)); // Back to dark blue text (#003366)
            m_clearBtn->Refresh();
        });
        clearBtnSizer->Add(m_clearBtn, 1, wxALL, 1); // 1 pixel border
        clearBtnPanel->SetSizer(clearBtnSizer);

        btns->Add(addBtnPanel, 0, wxRIGHT, 8);
        btns->Add(clearBtnPanel, 0);
        
        // Add buttons centered
        contentSizer->Add(btns, 0, wxALIGN_CENTER | wxBOTTOM, 8);
        
        centeringSizer->Add(contentSizer, 0, wxALIGN_CENTER_HORIZONTAL);
        centeringSizer->AddStretchSpacer(1);
        
        scrolledWindow->SetSizer(centeringSizer);

    // Calculate and set virtual size for proper scrolling
    wxSize virtualSize = centeringSizer->GetMinSize();
    scrolledWindow->SetVirtualSize(virtualSize);
    scrolledWindow->FitInside();
    scrolledWindow->SetScrollRate(5, 20);

    // Add scrolled window to main sizer with proportion=0 so it doesn't expand
    topSizer->Add(scrolledWindow, 0, wxEXPAND | wxBOTTOM, 8);

        // No list control - form only layout

            panel->SetSizer(topSizer);
            SetSize(1000, 600); // Increased window size to better show centered content
            Centre();

            // Bind events
            m_addBtn->Bind(wxEVT_BUTTON, &MyFrame::OnAddDevice, this);
            m_clearBtn->Bind(wxEVT_BUTTON, &MyFrame::OnClearFields, this);
            log_debug("MyFrame constructor completed successfully");
        } catch (const std::exception& e) {
            log_debug("Exception in MyFrame constructor: " + std::string(e.what()));
            throw; // Re-throw to be caught by OnInit
        } catch (...) {
            log_debug("Unknown exception in MyFrame constructor");
            throw; // Re-throw to be caught by OnInit
        }
    }

private:
    bool ValidateField(const std::string& value, const std::string& fieldName, wxStaticText* errorCtrl, 
                     const std::vector<FormValidator::ValidationResult(*)(const std::string&, const std::string&)>& validators) {
        bool isValid = true;
        for (auto validator : validators) {
            auto result = validator(value, fieldName);
            if (!result) {
                errorCtrl->SetLabel(result.message);
                isValid = false;
                break;
            }
        }
        if (isValid) {
            errorCtrl->SetLabel("");
        }
        return isValid;
    }

    void OnAddDevice(wxCommandEvent&)
    {
        bool hasErrors = false;

        // Clear all error messages first
        m_operatorIdError->SetLabel("");
        m_instanceIdError->SetLabel("");
        m_appVersionError->SetLabel("");
        m_deviceIdError->SetLabel("");
        m_deviceNameError->SetLabel("");
        m_statusError->SetLabel("");
        m_actionTypeError->SetLabel("");
        m_voltageError->SetLabel("");
        m_temperatureError->SetLabel("");
        m_severityError->SetLabel("");
        m_uiLatencyError->SetLabel("");
        m_notesError->SetLabel("");

        // Collect and validate each field
        const std::string operatorId = std::string(m_operatorId->GetValue().ToUTF8());
        if (!ValidateField(operatorId, "Operator ID", m_operatorIdError, {
            FormValidator::required,
            [](const auto& v, const auto& n) { return FormValidator::lengthRange(v, 1, 64, n); },
            [](const auto& v, const auto& n) { return FormValidator::regex(v, "^[a-zA-Z0-9_.-]+$", n); }
        })) hasErrors = true;

        const std::string instanceId = std::string(m_instanceId->GetValue().ToUTF8());
        if (!ValidateField(instanceId, "Instance ID", m_instanceIdError, {
            [](const auto& v, const auto& n) { return FormValidator::lengthRange(v, 1, 64, n); }
        })) hasErrors = true;

        const std::string appVersion = std::string(m_appVersion->GetValue().ToUTF8());
        if (!ValidateField(appVersion, "App Version", m_appVersionError, {
            [](const auto& v, const auto& n) { return FormValidator::lengthRange(v, 0, 32, n); }
        })) hasErrors = true;

        const std::string deviceId = std::string(m_deviceId->GetValue().ToUTF8());
        if (!ValidateField(deviceId, "Device ID", m_deviceIdError, {
            FormValidator::required
        })) hasErrors = true;

        const std::string deviceName = std::string(m_deviceName->GetValue().ToUTF8());
        if (!ValidateField(deviceName, "Device Name", m_deviceNameError, {
            [](const auto& v, const auto& n) { return FormValidator::lengthRange(v, 0, 128, n); }
        })) hasErrors = true;

        const std::string status = std::string(m_status->GetStringSelection().ToUTF8());
        if (!ValidateField(status, "Status", m_statusError, {
            FormValidator::required,
            [](const auto& v, const auto& n) { return FormValidator::enumValue(v, {"Unknown", "Online", "Offline", "Degraded"}, n); }
        })) hasErrors = true;

        const std::string actionType = std::string(m_actionType->GetStringSelection().ToUTF8());
        if (!ValidateField(actionType, "Action Type", m_actionTypeError, {
            FormValidator::required,
            [](const auto& v, const auto& n) { return FormValidator::enumValue(v, {"Check", "Maintenance", "Repair", "Replace"}, n); }
        })) hasErrors = true;

        const std::string voltage = std::string(m_voltage->GetValue().ToUTF8());
        if (!ValidateField(voltage, "Voltage", m_voltageError, {
            [](const auto& v, const auto& n) { return FormValidator::floatRange(v, 0.0f, 10000.0f, n); }
        })) hasErrors = true;

        const std::string temperature = std::string(m_temperature->GetValue().ToUTF8());
        if (!ValidateField(temperature, "Temperature", m_temperatureError, {
            [](const auto& v, const auto& n) { return FormValidator::floatRange(v, -50.0f, 250.0f, n); }
        })) hasErrors = true;

        const std::string severity = std::string(m_severity->GetStringSelection().ToUTF8());
        if (!ValidateField(severity, "Severity", m_severityError, {
            [](const auto& v, const auto& n) { return FormValidator::enumValue(v, {"Low", "Medium", "High", "Critical"}, n); }
        })) hasErrors = true;

        const std::string uiLatency = std::string(m_uiLatency->GetValue().ToUTF8());
        if (!ValidateField(uiLatency, "UI Latency", m_uiLatencyError, {
            [](const auto& v, const auto& n) { return FormValidator::intRange(v, 0, 600000, n); }
        })) hasErrors = true;

        const std::string notes = std::string(m_notes->GetValue().ToUTF8());
        if (!ValidateField(notes, "Notes", m_notesError, {
            [](const auto& v, const auto& n) { return FormValidator::lengthRange(v, 0, 500, n); }
        })) hasErrors = true;

        if (hasErrors) {
            return; // Don't proceed if there are validation errors
        }

        // Disable add button while generating defaults in background
        m_addBtn->Disable();

        // Launch background task to generate UUID and timestamp
        auto fut = std::async(std::launch::async, [=]() {
            // Simulate light background work
            std::string uuid = generate_uuid_v4();
            std::string ts = current_timestamp();
            return std::make_pair(uuid, ts);
        });

        // When done, save to CSV on main thread
        std::thread([this, fut = std::move(fut), operatorId, instanceId, appVersion, deviceId, deviceName, status, actionType, voltage, temperature, severity, uiLatency, notes]() mutable {
            auto pair = fut.get();
            const std::string uuid = pair.first;
            const std::string ts = pair.second;

            wxTheApp->CallAfter([=]() {
                
                        // Build CSV row (escape fields as needed)
                std::string row;
                row += csv_escape(uuid); row += ',';
                row += csv_escape(ts); row += ',';
                row += csv_escape(operatorId); row += ',';
                row += csv_escape(instanceId); row += ',';
                row += csv_escape(appVersion); row += ',';
                row += csv_escape(deviceId); row += ',';
                row += csv_escape(deviceName); row += ',';
                row += csv_escape(status); row += ',';
                row += csv_escape(actionType); row += ',';
                row += csv_escape(voltage); row += ',';
                row += csv_escape(temperature); row += ',';
                row += csv_escape(severity); row += ',';
                row += csv_escape(uiLatency); row += ',';
                row += csv_escape(notes);

                // Save to devices.csv in project folder
                try {
                    save_device_csv(get_appdata_devices_path(), row);
                    wxMessageBox("Device added successfully!", "Success", wxOK | wxICON_INFORMATION);
                } catch (...) {
                    wxMessageBox("Failed to save device data", "Error", wxOK | wxICON_ERROR);
                }
                
                m_addBtn->Enable();
            });
        }).detach();
    }

    void OnClearFields(wxCommandEvent&)
    {
        m_operatorId->Clear();
        m_instanceId->Clear();
        m_appVersion->SetValue("1.0.0");
        m_deviceId->Clear();
        m_deviceName->Clear();
        m_status->SetSelection(0);
        m_actionType->SetSelection(0);
        m_voltage->Clear();
        m_temperature->Clear();
        m_severity->SetSelection(0);
        m_uiLatency->SetValue("0");
        m_notes->Clear();

        // Clear all error messages
        m_operatorIdError->SetLabel("");
        m_instanceIdError->SetLabel("");
        m_appVersionError->SetLabel("");
        m_deviceIdError->SetLabel("");
        m_deviceNameError->SetLabel("");
        m_statusError->SetLabel("");
        m_actionTypeError->SetLabel("");
        m_voltageError->SetLabel("");
        m_temperatureError->SetLabel("");
        m_severityError->SetLabel("");
        m_uiLatencyError->SetLabel("");
        m_notesError->SetLabel("");
    }



    // Controls
    wxTextCtrl* m_operatorId{nullptr};
    wxTextCtrl* m_instanceId{nullptr};
    wxTextCtrl* m_appVersion{nullptr};
    wxTextCtrl* m_deviceId{nullptr};
    wxTextCtrl* m_deviceName{nullptr};
    wxChoice* m_status{nullptr};
    wxChoice* m_actionType{nullptr};
    wxTextCtrl* m_voltage{nullptr};
    wxTextCtrl* m_temperature{nullptr};
    wxChoice* m_severity{nullptr};
    wxTextCtrl* m_uiLatency{nullptr};
    wxTextCtrl* m_notes{nullptr};
    wxButton* m_addBtn{nullptr};
    wxButton* m_clearBtn{nullptr};
    // Error message controls
    wxStaticText* m_operatorIdError{nullptr};
    wxStaticText* m_instanceIdError{nullptr};
    wxStaticText* m_appVersionError{nullptr};
    wxStaticText* m_deviceIdError{nullptr};
    wxStaticText* m_deviceNameError{nullptr};
    wxStaticText* m_statusError{nullptr};
    wxStaticText* m_actionTypeError{nullptr};
    wxStaticText* m_voltageError{nullptr};
    wxStaticText* m_temperatureError{nullptr};
    wxStaticText* m_severityError{nullptr};
    wxStaticText* m_uiLatencyError{nullptr};
    wxStaticText* m_notesError{nullptr};
};

class MyApp : public wxApp
{
public:
    void OnAssertFailure(const wxChar* file, int line, const wxChar* func,
                        const wxChar* cond, const wxChar* msg) override {
        std::stringstream ss;
        ss << "Assert failed in " << (func ? wxString(func).ToStdString() : "unknown function") 
           << " at " << (file ? wxString(file).ToStdString() : "unknown file") << ":" << line
           << "\nCondition: " << (cond ? wxString(cond).ToStdString() : "unknown")
           << "\nMessage: " << (msg ? wxString(msg).ToStdString() : "none");
        log_debug(ss.str());
    }

    bool OnInit() override
    {
        try {
            log_debug("Application starting");
            // Enable call stack traces
            wxHandleFatalExceptions();
            
            MyFrame* frame = new MyFrame();
            if (!frame) {
                log_debug("Failed to create frame");
                return false;
            }
            
            log_debug("Frame created successfully");
            frame->Show(true);
            log_debug("Frame shown");
            return true;
        } catch (const std::exception& e) {
            log_debug("Exception during initialization: " + std::string(e.what()));
            wxMessageBox(wxString::Format("Error initializing application: %s", e.what()),
                        "Error", wxOK | wxICON_ERROR);
        } catch (...) {
            log_debug("Unknown exception during initialization");
            wxMessageBox("Unknown error initializing application",
                        "Error", wxOK | wxICON_ERROR);
        }
        return false;
    }
};

wxIMPLEMENT_APP(MyApp);

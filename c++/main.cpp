#include <wx/wx.h>
#include <thread>
#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <atomic>

#include <windows.h>
#include "vcinpl.h" // The correct native C-API Header

// --- Bit Shifts ---
constexpr int DESTIDSHIFT = 0;
constexpr int CMDSHIFT = 8;
constexpr int CPUEDGESHIFT = 16;
constexpr int FORMATSHIFT = 18;
constexpr int CMDCATSHIFT = 20;
constexpr int TYPESHIFT = 24;

// --- Data Structures ---
struct CanMessage {
    uint32_t arbitration_id;
    std::vector<uint8_t> data;
    bool is_extended_id;
    double timestamp;
};

// --- Real IXXAT CAN Interface ---
class IXXATBus {
private:
    HANDLE hDevice;
    HANDLE hCanCtl; // Control Handle
    HANDLE hCanChl; // Message Channel Handle
    bool initialized;

public:
    IXXATBus() : hDevice(NULL), hCanCtl(NULL), hCanChl(NULL), initialized(false) {
        Init();
    }

    ~IXXATBus() {
        if (hCanChl) {
            canChannelActivate(hCanChl, FALSE);
            canChannelClose(hCanChl);
        }
        if (hCanCtl) {
            canControlStart(hCanCtl, FALSE);
            canControlClose(hCanCtl);
        }
        if (hDevice) vciDeviceClose(hDevice);
    }

    void Init() {
        // Initialize VCI
        if (vciInitialize() != VCI_OK) return;

        // Find the first device
        HANDLE hEnum;
        if (vciEnumDeviceOpen(&hEnum) != VCI_OK) return;

        VCIDEVICEINFO devInfo;
        if (vciEnumDeviceNext(hEnum, &devInfo) != VCI_OK) {
            vciEnumDeviceClose(hEnum);
            return;
        }
        vciEnumDeviceClose(hEnum);

        // Open the physical device
        if (vciDeviceOpen(devInfo.VciObjectId, &hDevice) != VCI_OK) return;

        // Open and initialize the Control Unit for 1 Mbps (BTR0=0x00, BTR1=0x14)
        if (canControlOpen(hDevice, 0, &hCanCtl) != VCI_OK) return;
        if (canControlInitialize(hCanCtl, CAN_OPMODE_STANDARD | CAN_OPMODE_EXTENDED, 0x00, 0x14) != VCI_OK) return;
        if (canControlStart(hCanCtl, TRUE) != VCI_OK) return;

        // Open and initialize the Message Channel (FIFO size 1024, threshold 1)
        if (canChannelOpen(hDevice, 0, FALSE, &hCanChl) != VCI_OK) return;
        if (canChannelInitialize(hCanChl, 1024, 1, 1024, 1) != VCI_OK) return;
        if (canChannelActivate(hCanChl, TRUE) != VCI_OK) return;

        initialized = true;
    }

    bool isReady() const { return initialized; }

    void send(const CanMessage& msg) {
        if (!initialized) return;

        CANMSG vciMsg = { 0 };
        vciMsg.dwTime   = 0;
        vciMsg.dwMsgId  = msg.arbitration_id;
        vciMsg.uMsgInfo.Bits.dlc = msg.data.size() > 8 ? 8 : msg.data.size();
        vciMsg.uMsgInfo.Bits.type = CAN_MSGTYPE_DATA;
        vciMsg.uMsgInfo.Bits.ext = msg.is_extended_id ? 1 : 0;

        for (size_t i = 0; i < vciMsg.uMsgInfo.Bits.dlc; ++i) {
            vciMsg.abData[i] = msg.data[i];
        }

        canChannelSendMessage(hCanChl, INFINITE, &vciMsg);
    }

    bool recv(CanMessage& msg) {
        if (!initialized) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return false;
        }

        CANMSG vciMsg;
        // Wait up to 100ms for a message
        HRESULT hr = canChannelReadMessage(hCanChl, 100, &vciMsg); 
        
        if (hr == VCI_OK && vciMsg.uMsgInfo.Bits.type == CAN_MSGTYPE_DATA) {
            msg.arbitration_id = vciMsg.dwMsgId;
            msg.is_extended_id = vciMsg.uMsgInfo.Bits.ext == 1;
            msg.data.assign(vciMsg.abData, vciMsg.abData + vciMsg.uMsgInfo.Bits.dlc);
            msg.timestamp = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
            return true;
        }
        return false;
    }
};

IXXATBus bus;

// --- Global State ---
std::atomic<bool> MAP_ON{false};
std::map<int, std::pair<int, wxString>> modules;
std::mutex modules_mutex;

// --- Helper Functions ---
wxString GetCurrentTimeStr(double timestamp) {
    auto t = static_cast<time_t>(timestamp);
    struct tm* tm_info = localtime(&t);
    char buffer[26];
    strftime(buffer, 26, "%H:%M:%S", tm_info);
    return wxString(buffer);
}

wxString BytesToHex(const std::vector<uint8_t>& data) {
    wxString res;
    for (uint8_t b : data) {
        res += wxString::Format("%02x", b);
    }
    return res;
}

wxString FormatLookup(int cmdcat, int cmd, const std::vector<int>& args) {
    static std::vector<std::vector<wxString>> lookup = {
        {"", "(Config Aware", "T{:x}E{:x}i{:x}Q{:x} (Query", "T{:x}E{:x}i{:x}S{:x} (Set ID", "T{:x}E{:x}i{:x}R{:x} (Set Relay"},
        {"T{:x}E{:x}i{:x}C{:x}{:x}N{:x}{:x} (TopoMUX", "T{:x}E{:x}i{:x}P{:x} (TopoPower Cold", "T{:x}E{:x}i{:x}H{:x} (TopoPower Hot", "T{:x}E{:x}i{:x}t{:x}B{:x}l{:x}r{:x} (Power Status"},
        {"T{:x}E{:x}i{:x}I{:x}o{:x} (IO Command"}
    };

    int cat = cmdcat % 8;
    if (cat >= lookup.size() || cmd >= lookup[cat].size() || lookup[cat][cmd].IsEmpty()) {
        return "(Unknown Command";
    }

    wxString fmt = lookup[cat][cmd];
    wxString result;
    size_t arg_idx = 0;
    
    for (size_t k = 0; k < fmt.Length(); ++k) {
        if (k + 3 <= fmt.Length() && fmt.Mid(k, 4) == "{:x}") {
            if (arg_idx < args.size()) {
                result += wxString::Format("%x", args[arg_idx++]);
            }
            k += 3; 
        } else {
            result += fmt[k];
        }
    }
    return result;
}

// --- GUI Window ---
class MyFrame : public wxFrame {
public:
    MyFrame(const wxString& title) : wxFrame(NULL, wxID_ANY, title, wxDefaultPosition, wxSize(800, 750)) {
        wxPanel* panel = new wxPanel(this);

        wxFont modernFont9(9, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        wxFont modernFont10(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        wxFont modernFont13(13, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        wxFont modernFont15(15, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        wxFont modernFont25(25, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

        auto labelID = new wxStaticText(panel, wxID_ANY, "Identifier", wxPoint(50, 30));
        labelID->SetFont(modernFont9);
        idInput = new wxTextCtrl(panel, wxID_ANY, "00000000", wxPoint(50, 50), wxSize(180, 50), wxTE_RIGHT);
        idInput->SetFont(modernFont25);

        int startX = 230;
        for (int i = 0; i < 8; ++i) {
            auto label = new wxStaticText(panel, wxID_ANY, wxString::Format("Byte %d", i + 1), wxPoint(startX + (i * 50), 30));
            label->SetFont(modernFont9);
            byteInputs[i] = new wxTextCtrl(panel, wxID_ANY, "00", wxPoint(startX + (i * 50), 50), wxSize(50, 50), wxTE_RIGHT);
            byteInputs[i]->SetFont(modernFont25);
        }

        wxButton* sendBtn = new wxButton(panel, wxID_ANY, "SEND", wxPoint(650, 50), wxSize(100, 50));
        sendBtn->SetFont(modernFont15);
        sendBtn->Bind(wxEVT_BUTTON, &MyFrame::OnSend, this);

        errorLabel = new wxStaticText(panel, wxID_ANY, "", wxPoint(50, 120));
        errorLabel->SetFont(modernFont9);

        mapCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(50, 150), wxSize(700, 450), wxTE_MULTILINE | wxTE_READONLY | wxVSCROLL);
        mapCtrl->SetFont(modernFont13);
        mapCtrl->Hide();

        logsCtrl = new wxTextCtrl(panel, wxID_ANY, "", wxPoint(50, 150), wxSize(700, 450), wxTE_MULTILINE | wxTE_READONLY | wxVSCROLL);
        logsCtrl->SetFont(modernFont10);

        showMapBtn = new wxButton(panel, wxID_ANY, "LOGS", wxPoint(0, 0), wxSize(50, 25));
        showMapBtn->SetFont(modernFont9);
        showMapBtn->Bind(wxEVT_BUTTON, &MyFrame::OnMapToggle, this);

        fileLocation = new wxTextCtrl(panel, wxID_ANY, "C:\\", wxPoint(180, 620), wxSize(440, 25), wxTE_LEFT);
        
        wxButton* saveBtn = new wxButton(panel, wxID_ANY, "SAVE", wxPoint(50, 620), wxSize(100, 50));
        saveBtn->SetFont(modernFont15);
        saveBtn->Bind(wxEVT_BUTTON, &MyFrame::OnSave, this);

        wxButton* clearBtn = new wxButton(panel, wxID_ANY, "CLEAR", wxPoint(650, 620), wxSize(100, 50));
        clearBtn->SetFont(modernFont15);
        clearBtn->Bind(wxEVT_BUTTON, &MyFrame::OnClear, this);

        saveErrorLabel = new wxStaticText(panel, wxID_ANY, "", wxPoint(180, 650));
        saveErrorLabel->SetFont(modernFont9);

        running = true;
        workerThread = std::thread(&MyFrame::BackgroundReadTask, this);
    }

    ~MyFrame() {
        running = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    void AppendLogThreadSafe(const wxString& logMsg) {
        this->CallAfter([this, logMsg]() {
            this->logsCtrl->AppendText(logMsg);
        });
    }

private:
    wxTextCtrl* idInput;
    wxTextCtrl* byteInputs[8];
    wxStaticText* errorLabel;
    wxTextCtrl* mapCtrl;
    wxTextCtrl* logsCtrl;
    wxButton* showMapBtn;
    wxTextCtrl* fileLocation;
    wxStaticText* saveErrorLabel;

    std::thread workerThread;
    std::atomic<bool> running;

    void OnSend(wxCommandEvent& event) {
        try {
            long id_val;
            if (!idInput->GetValue().ToLong(&id_val, 16) || id_val > 0x1fffffff) {
                throw std::runtime_error("Invalid ID");
            }

            std::vector<uint8_t> data(8);
            for (int i = 0; i < 8; ++i) {
                long b_val;
                if (!byteInputs[i]->GetValue().ToLong(&b_val, 16)) throw std::runtime_error("Invalid Byte");
                data[i] = static_cast<uint8_t>(b_val);
            }

            CanMessage tosend;
            tosend.arbitration_id = static_cast<uint32_t>(id_val);
            tosend.data = data;
            tosend.is_extended_id = (id_val > 4095);
            tosend.timestamp = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();

            bus.send(tosend);

            int cmd = (id_val >> CMDSHIFT) & 0xFF;
            int cmdcat = (id_val >> CMDCATSHIFT) & 0xF;
            int format = (id_val >> FORMATSHIFT) & 0x3;
            int t = (id_val >> TYPESHIFT);
            int i_val = id_val & 0xFF;
            int e = (id_val >> CPUEDGESHIFT) & 0x3;

            std::vector<int> d(16, 0);
            if (format == 0) {
                for (int j = 0; j < 8; ++j) d[j] = data[j];
            } else if (format == 1) {
                for (int k = 0; k < 8; ++k) {
                    d[2 * k] = data[k] >> 4;
                    d[2 * k + 1] = data[k] & 0xF;
                }
            }

            std::vector<int> args = {t, e + 1, i_val, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]};
            wxString lookupStr = FormatLookup(cmdcat, cmd, args);
            wxString replyStr = (cmdcat >= 8) ? " Reply" : "";

            wxString logLine = wxString::Format("%s Sent:     %010x 0x%s %s%s)\n",
                GetCurrentTimeStr(tosend.timestamp),
                tosend.arbitration_id,
                BytesToHex(tosend.data),
                lookupStr,
                replyStr);

            logsCtrl->AppendText(logLine);

            errorLabel->SetLabel("SENT!");
            errorLabel->SetBackgroundColour(*wxGREEN);
            errorLabel->SetForegroundColour(*wxBLACK);

        } catch (...) {
            errorLabel->SetLabel("ERROR: Invalid data frame!");
            errorLabel->SetBackgroundColour(*wxRED);
            errorLabel->SetForegroundColour(*wxWHITE);
        }
    }

    void OnSave(wxCommandEvent& event) {
        wxString basePath = fileLocation->GetValue();
        wxString logsPath = basePath + "\\logs.txt";
        wxString mapPath = basePath + "\\map.txt";

        std::ofstream fileLogs(logsPath.mb_str());
        std::ofstream fileMap(mapPath.mb_str());

        if (fileLogs.is_open() && fileMap.is_open()) {
            fileLogs << logsCtrl->GetValue().ToStdString();
            fileMap << mapCtrl->GetValue().ToStdString();
            saveErrorLabel->SetLabel(wxString::Format("Successfully saved to %s", basePath));
            saveErrorLabel->SetBackgroundColour(*wxGREEN);
            saveErrorLabel->SetForegroundColour(*wxBLACK);
        } else {
            saveErrorLabel->SetLabel("Error saving files.");
            saveErrorLabel->SetBackgroundColour(*wxRED);
            saveErrorLabel->SetForegroundColour(*wxWHITE);
        }
    }

    void OnClear(wxCommandEvent& event) {
        logsCtrl->Clear();
    }

    void OnMapToggle(wxCommandEvent& event) {
        MAP_ON = !MAP_ON;
        if (MAP_ON) {
            showMapBtn->SetLabel("LOGS");
            errorLabel->SetFont(wxFont(13, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
            errorLabel->SetLabel(wxString::Format("%-6s%-6s%-6s%-6s%-6s%-6s%-6s%-6s%-6s%-6s", "ID", "TYPE", "TOP", "FACE", "RIGHT", "FACE", "BOTTOM", "FACE", "LEFT", "FACE"));
            errorLabel->SetBackgroundColour(*wxWHITE);
            errorLabel->SetForegroundColour(*wxBLACK);

            mapCtrl->Clear();
            wxString dir = "TBLR";

            std::lock_guard<std::mutex> lock(modules_mutex);
            for (const auto& [module, data] : modules) {
                int t = data.first;
                wxString hexData = data.second; 

                if (hexData.Length() >= 16) {
                    long t_id, t_face, r_id, r_face, b_id, b_face, l_id, l_face;
                    hexData.Mid(0, 2).ToLong(&t_id, 16);
                    hexData.Mid(8, 2).ToLong(&t_face, 16);
                    hexData.Mid(2, 2).ToLong(&r_id, 16);
                    hexData.Mid(10, 2).ToLong(&r_face, 16);
                    hexData.Mid(4, 2).ToLong(&b_id, 16);
                    hexData.Mid(12, 2).ToLong(&b_face, 16);
                    hexData.Mid(6, 2).ToLong(&l_id, 16);
                    hexData.Mid(14, 2).ToLong(&l_face, 16);

                    wxString tid_s = (t_id == 99) ? "-" : wxString::Format("%ld", t_id);
                    wxString tface_s = (t_id == 99 || t_face > 3) ? "-" : wxString(dir[t_face]);

                    wxString rid_s = (r_id == 99) ? "-" : wxString::Format("%ld", r_id);
                    wxString rface_s = (r_id == 99 || r_face > 3) ? "-" : wxString(dir[r_face]);

                    wxString bid_s = (b_id == 99) ? "-" : wxString::Format("%ld", b_id);
                    wxString bface_s = (b_id == 99 || b_face > 3) ? "-" : wxString(dir[b_face]);

                    wxString lid_s = (l_id == 99) ? "-" : wxString::Format("%ld", l_id);
                    wxString lface_s = (l_id == 99 || l_face > 3) ? "-" : wxString(dir[l_face]);

                    mapCtrl->AppendText(wxString::Format("%-5d|%-5d|%-5s|%-5s|%-5s|%-5s|%-5s|%-5s|%-5s|%-5s\n", 
                                          module, t, tid_s, tface_s, rid_s, rface_s, bid_s, bface_s, lid_s, lface_s));
                }
            }

            mapCtrl->Show();
            logsCtrl->Hide();
        } else {
            showMapBtn->SetLabel("MAP");
            errorLabel->SetFont(wxFont(9, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
            errorLabel->SetLabel("");
            logsCtrl->Show();
            mapCtrl->Hide();
        }
        this->Layout();
    }

    void BackgroundReadTask() {
        while (running) {
            CanMessage msg;
            if (bus.recv(msg)) {
                int cmd = (msg.arbitration_id >> CMDSHIFT) & 0xFF;
                int cmdcat = (msg.arbitration_id >> CMDCATSHIFT) & 0xF;
                int i_val = msg.arbitration_id & 0xFF;
                int e = (msg.arbitration_id >> CPUEDGESHIFT) & 0x3;
                int t = (msg.arbitration_id >> TYPESHIFT);
                int format = (msg.arbitration_id >> FORMATSHIFT) & 0x3;

                std::vector<int> d(16, 0);
                if (format == 0 && msg.data.size() >= 8) {
                    for (int j = 0; j < 8; ++j) d[j] = msg.data[j];
                } else if (format == 1 && msg.data.size() >= 8) {
                    for (int k = 0; k < 8; ++k) {
                        d[2 * k] = msg.data[k] >> 4;
                        d[2 * k + 1] = msg.data[k] & 0xF;
                    }
                }

                std::vector<int> args = {t, e + 1, i_val, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]};
                wxString lookupStr = FormatLookup(cmdcat, cmd, args);
                wxString replyStr = (cmdcat >= 8) ? " Reply" : "";

                wxString logLine = wxString::Format("%s Received: %010x 0x%s %s%s)\n",
                    GetCurrentTimeStr(msg.timestamp),
                    msg.arbitration_id,
                    BytesToHex(msg.data),
                    lookupStr,
                    replyStr);

                AppendLogThreadSafe(logLine);

                if (cmd == 1 && cmdcat == 0) {
                    std::lock_guard<std::mutex> lock(modules_mutex);
                    modules[i_val] = {t, BytesToHex(msg.data)};
                }
            }
        }
    }
};

class MyApp : public wxApp {
public:
    virtual bool OnInit() {
        MyFrame* frame = new MyFrame("CAN Logger");
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);

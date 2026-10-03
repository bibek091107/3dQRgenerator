#include "App.h"
#include "QRGenerator.h"
#include "MeshBuilder.h"
#include "STLWriter.h"
#include "PNGWriter.h"
// Shared generation engine (no OpenGL / ImGui code inside these)
#include "EngineService.h"
#include "PayloadBuilder.h"
#include "OutputPaths.h"
#include "Validation.h"

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// ═══════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════

static inline void Trim(std::string& s) {
    s.erase(s.begin(),
        std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); }));
    s.erase(
        std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base(),
        s.end());
}

// ═══════════════════════════════════════════════════════════════
//  Constructor / Destructor
// ═══════════════════════════════════════════════════════════════

App::App()
    : m_State(AppState::MainMenu), m_Window(nullptr)
{}

App::~App() { Cleanup(); }

// ═══════════════════════════════════════════════════════════════
//  Window / ImGui lifecycle
// ═══════════════════════════════════════════════════════════════

void App::InitWindow() {
    if (!glfwInit()) { std::cerr << "[ERROR] glfwInit failed\n"; exit(1); }
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    m_Window = glfwCreateWindow(1280, 800, "3D QR Code Generator", NULL, NULL);
    if (!m_Window) { glfwTerminate(); exit(1); }
    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
}

void App::InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void App::Cleanup() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (m_Window) glfwDestroyWindow(m_Window);
    glfwTerminate();
}

// ═══════════════════════════════════════════════════════════════
//  Path helpers — filesystem source of truth
// ═══════════════════════════════════════════════════════════════

fs::path App::GetSTLPath(const QRObject& obj) const {
    return fs::path(outputpaths::STLRelativePath(obj.type, obj.id));
}

fs::path App::GetPNGPath(const QRObject& obj) const {
    return fs::path(outputpaths::PNGRelativePath(obj.type, obj.id));
}

// Checks in-memory session set AND actual filesystem.
// A duplicate means either file (STL or PNG) already exists in the correct folder.
bool App::IDAlreadyExists(const std::string& id, const std::string& type) const {
    // 1. Session check (fast)
    if (m_UsedIDs.count(id)) return true;

    // 2. Filesystem check (persistent across restarts)
    bool exists = outputpaths::IDExistsOnDisk(fs::current_path(), type, id);

    std::cout << "[DUP CHECK] " << outputpaths::FolderRelativePath(type) << "/" << id
              << "  exists=" << exists << "\n";

    return exists;
}

// ═══════════════════════════════════════════════════════════════
//  Main loop
// ═══════════════════════════════════════════════════════════════

void App::Run() {
    InitWindow();
    InitImGui();
    m_Renderer.Init();

    // Ensure output directories always exist
    engine::EnsureOutputDirs(fs::current_path());
    std::cout << "[INFO] Output dirs: "
              << fs::absolute("output/holder").string() << "\n"
              << "                    "
              << fs::absolute("output/tools").string()  << "\n";

    double lastX = 0, lastY = 0;
    glfwGetCursorPos(m_Window, &lastX, &lastY);

    while (!glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();

        if (!ImGui::GetIO().WantCaptureMouse) {
            double mx, my;
            glfwGetCursorPos(m_Window, &mx, &my);
            float dx = (float)(mx - lastX);
            float dy = (float)(my - lastY);
            if (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_LEFT)   == GLFW_PRESS)
                m_Camera.ProcessMouseDrag(dx, dy);
            if (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
                m_Camera.ProcessMousePan(dx, dy);
            lastX = mx; lastY = my;
        } else {
            glfwGetCursorPos(m_Window, &lastX, &lastY);
        }

        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Upload mesh on preview change
        if (m_PreviewUpdated && m_State == AppState::Preview &&
            m_PreviewIndex >= 0 && m_PreviewIndex < (int)m_PendingObjects.size())
        {
            m_Renderer.SetMesh(m_PendingObjects[m_PreviewIndex].mesh);
            m_PreviewUpdated = false;
        } else if (m_State != AppState::Preview) {
            m_Renderer.SetMesh(Mesh{});
        }

        int fw, fh;
        glfwGetFramebufferSize(m_Window, &fw, &fh);
        float aspect = (float)fw / (float)(fh > 0 ? fh : 1);
        m_Renderer.Draw(m_Camera.GetViewMatrix(), m_Camera.GetProjectionMatrix(aspect));

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        RenderUI();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_Window);
    }
}

// ═══════════════════════════════════════════════════════════════
//  UI dispatch
// ═══════════════════════════════════════════════════════════════

void App::ResetHolderInputBuffers() {
    memset(m_InputID,                0, sizeof(m_InputID));
    memset(m_InputName,              0, sizeof(m_InputName));
    memset(m_InputCalibrationStatus, 0, sizeof(m_InputCalibrationStatus));
    memset(m_InputAccumulatedLife,   0, sizeof(m_InputAccumulatedLife));
    memset(m_InputSerialBatchNumber, 0, sizeof(m_InputSerialBatchNumber));
}

void App::ResetToolInputBuffers() {
    memset(m_InputID,              0, sizeof(m_InputID));
    memset(m_InputName,            0, sizeof(m_InputName));
    memset(m_InputToolLife,        0, sizeof(m_InputToolLife));
    memset(m_InputCompletedLife,   0, sizeof(m_InputCompletedLife));
    memset(m_InputRemainingVisual, 0, sizeof(m_InputRemainingVisual));
    memset(m_InputRemainingML,     0, sizeof(m_InputRemainingML));
    memset(m_InputTotalUsageHours, 0, sizeof(m_InputTotalUsageHours));
    memset(m_InputCncMachineId,    0, sizeof(m_InputCncMachineId));
    memset(m_InputCncMachineName,  0, sizeof(m_InputCncMachineName));
    memset(m_InputOperationType,   0, sizeof(m_InputOperationType));
    memset(m_InputTrayNo,          0, sizeof(m_InputTrayNo));
}

void App::RenderUI() {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::Begin("Control Panel", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);

    switch (m_State) {
        case AppState::MainMenu:      DrawMainMenu();          break;
        case AppState::HolderQuantity:
        case AppState::ToolQuantity:  DrawQuantityMenu();      break;
        case AppState::HolderDetails: DrawHolderDetailsMenu(); break;
        case AppState::ToolDetails:   DrawToolDetailsMenu();   break;
        case AppState::SizeSelection: DrawSizeSelection();     break;
        case AppState::Preview:       DrawPreviewMenu();       break;
    }

    DrawPopups();
    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════
//  Main Menu
// ═══════════════════════════════════════════════════════════════

void App::DrawMainMenu() {
    ImGui::Text("====================================");
    ImGui::Text("      3D QR CODE GENERATOR         ");
    ImGui::Text("====================================");
    ImGui::Spacing();
    ImGui::Text("Choose Material Type:");
    ImGui::Spacing();
    if (ImGui::Button("  HOLDER  ", ImVec2(160, 36))) {
        m_CurrentType = "Holder";
        m_State = AppState::HolderQuantity;
        memset(m_QuantityStr, 0, sizeof(m_QuantityStr));
    }
    if (ImGui::Button("  TOOL    ", ImVec2(160, 36))) {
        m_CurrentType = "Tool";
        m_State = AppState::ToolQuantity;
        memset(m_QuantityStr, 0, sizeof(m_QuantityStr));
    }
    ImGui::Spacing();
    if (ImGui::Button("  EXIT    ", ImVec2(160, 36)))
        glfwSetWindowShouldClose(m_Window, true);
}

// ═══════════════════════════════════════════════════════════════
//  Quantity
// ═══════════════════════════════════════════════════════════════

void App::DrawQuantityMenu() {
    ImGui::Text("Type: %s", m_CurrentType.c_str());
    ImGui::Separator();
    ImGui::Text("How many QR codes do you want to generate?");
    ImGui::InputText("Quantity", m_QuantityStr, sizeof(m_QuantityStr),
                     ImGuiInputTextFlags_CharsDecimal);
    ImGui::Spacing();
    if (ImGui::Button("Next", ImVec2(100, 0))) {
        int qty = atoi(m_QuantityStr);
        if (qty > 0) {
            m_TargetQuantity = qty;
            m_CurrentIndex   = 0;
            m_PendingObjects.clear();
            m_UsedIDs.clear();
            if (m_CurrentType == "Holder") {
                m_State = AppState::HolderDetails;
                ResetHolderInputBuffers();
            } else {
                m_State = AppState::ToolDetails;
                ResetToolInputBuffers();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(100, 0))) m_State = AppState::MainMenu;
}

// ═══════════════════════════════════════════════════════════════
//  Holder Details Entry
// ═══════════════════════════════════════════════════════════════

void App::DrawHolderDetailsMenu() {
    ImGui::Text("====================================");
    ImGui::Text("        HOLDER INFORMATION          ");
    ImGui::Text("====================================");
    ImGui::Text("Holder %d of %d", m_CurrentIndex + 1, m_TargetQuantity);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Holder ID:");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##HolderID",   m_InputID,   sizeof(m_InputID));

    ImGui::Text("Holder Name:");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##HolderName", m_InputName, sizeof(m_InputName));
    ImGui::Spacing();

    ImGui::Text("Calibration / Inspection Status:");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##CalibStatus", m_InputCalibrationStatus, sizeof(m_InputCalibrationStatus));
    ImGui::SameLine();
    if (ImGui::SmallButton("Passed"))     { strncpy(m_InputCalibrationStatus, "Passed", sizeof(m_InputCalibrationStatus) - 1); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Calibrated")) { strncpy(m_InputCalibrationStatus, "Calibrated", sizeof(m_InputCalibrationStatus) - 1); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Pending"))    { strncpy(m_InputCalibrationStatus, "Pending Inspection", sizeof(m_InputCalibrationStatus) - 1); }

    ImGui::Spacing();
    ImGui::Text("Current Accumulated Life (e.g. 120.5 mins / 450 parts cut):");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##AccumLife", m_InputAccumulatedLife, sizeof(m_InputAccumulatedLife));

    ImGui::Spacing();
    ImGui::Text("Serial Number / Batch Number (Traceability code):");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##SerialBatch", m_InputSerialBatchNumber, sizeof(m_InputSerialBatchNumber));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* btnLabel = (m_CurrentIndex + 1 >= m_TargetQuantity) ? "Save Holder" : "Next Holder";
    if (ImGui::Button(btnLabel, ImVec2(130, 32))) {
        std::string idStr(m_InputID);
        std::string nameStr(m_InputName);
        std::string calibStr(m_InputCalibrationStatus);
        std::string lifeStr(m_InputAccumulatedLife);
        std::string batchStr(m_InputSerialBatchNumber);
        Trim(idStr);
        Trim(nameStr);
        Trim(calibStr);
        Trim(lifeStr);
        Trim(batchStr);

        if (calibStr.empty()) calibStr = "N/A";
        if (lifeStr.empty())  lifeStr  = "N/A";
        if (batchStr.empty()) batchStr = "N/A";

        if (idStr.empty() || nameStr.empty()) {
            m_ShowEmptyFieldPopup = true;
        }
        else if (IDAlreadyExists(idStr, "Holder")) {
            m_DuplicateIDValue = idStr;
            m_ShowDuplicateIDPopup = true;
        }
        else {
            m_UsedIDs.insert(idStr);

            QRObject obj;
            obj.type              = "Holder";
            obj.id                = idStr;
            obj.name              = nameStr;
            obj.calibrationStatus = calibStr;
            obj.accumulatedLife   = lifeStr;
            obj.serialBatchNumber = batchStr;
            obj.stlGenerated      = false;
            obj.pngGenerated      = false;
            m_PendingObjects.push_back(obj);

            m_CurrentIndex++;
            if (m_CurrentIndex >= m_TargetQuantity) {
                m_State = AppState::SizeSelection;
            } else {
                ResetHolderInputBuffers();
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(100, 32))) {
        m_State = AppState::HolderQuantity;
    }
}

// ═══════════════════════════════════════════════════════════════
//  Tool Details Entry
// ═══════════════════════════════════════════════════════════════

void App::DrawToolDetailsMenu() {
    ImGui::Text("====================================");
    ImGui::Text("         TOOL INFORMATION           ");
    ImGui::Text("====================================");
    ImGui::Text("Tool %d of %d", m_CurrentIndex + 1, m_TargetQuantity);
    ImGui::Separator();
    ImGui::Spacing();

    // Field 1 — Tool ID
    ImGui::Text("Tool ID:");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##ToolID", m_InputID, sizeof(m_InputID));

    // Field 2 — Tool Name
    ImGui::Text("Tool Name:");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##ToolName", m_InputName, sizeof(m_InputName));
    ImGui::Spacing();

    // Field 3 — Tool Life (meters)
    ImGui::Text("Tool Life (meters):");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##ToolLife", m_InputToolLife, sizeof(m_InputToolLife), ImGuiInputTextFlags_CharsDecimal);

    // Field 4 — Completed Life (meters)
    ImGui::Text("Completed Life (meters):");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##CompletedLife", m_InputCompletedLife, sizeof(m_InputCompletedLife), ImGuiInputTextFlags_CharsDecimal);

    // Display expected remaining life helper calculation without overwriting user inputs
    {
        double tlVal = 0.0, clVal = 0.0;
        bool tlOk = false, clOk = false;
        try { if (strlen(m_InputToolLife) > 0) { tlVal = std::stod(m_InputToolLife); tlOk = true; } } catch (...) {}
        try { if (strlen(m_InputCompletedLife) > 0) { clVal = std::stod(m_InputCompletedLife); clOk = true; } } catch (...) {}

        if (tlOk && clOk && tlVal >= 0 && clVal >= 0) {
            double expectedRemaining = tlVal - clVal;
            if (expectedRemaining >= 0) {
                ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "  Expected Remaining Life: %.2f m (Tool Life - Completed Life)", expectedRemaining);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "  Warning: Completed Life exceeds Tool Life!");
            }
        }
    }
    ImGui::Spacing();

    // Field 5 — Remaining Life (A: Visual Inspection, B: ML Model)
    ImGui::Text("Remaining Life - Visual Inspection (meters):");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##RemainingVisual", m_InputRemainingVisual, sizeof(m_InputRemainingVisual), ImGuiInputTextFlags_CharsDecimal);

    ImGui::Text("Remaining Life - ML Model (meters):");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##RemainingML", m_InputRemainingML, sizeof(m_InputRemainingML), ImGuiInputTextFlags_CharsDecimal);
    ImGui::Spacing();

    // Field 6 — Total Hours of Usage
    ImGui::Text("Total Hours of Usage:");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##TotalHours", m_InputTotalUsageHours, sizeof(m_InputTotalUsageHours), ImGuiInputTextFlags_CharsDecimal);
    ImGui::Spacing();

    // Field 7 — CNC Machine (A: CNC Machine ID, B: CNC Machine Name)
    ImGui::Text("CNC Machine ID:");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##CncMachineId", m_InputCncMachineId, sizeof(m_InputCncMachineId));

    ImGui::Text("CNC Machine Name:");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##CncMachineName", m_InputCncMachineName, sizeof(m_InputCncMachineName));
    ImGui::Spacing();

    // Field 8 — Type of Operation (with arrow button + hierarchical dropdown + manual typing)
    ImGui::Text("Type of Operation:");
    ImGui::SetNextItemWidth(286);
    ImGui::InputText("##OperationType", m_InputOperationType, sizeof(m_InputOperationType));
    ImGui::SameLine();
    if (ImGui::Button("\xE2\x96\xBC##OpDropdownBtn", ImVec2(28, 0))) {
        ImGui::OpenPopup("OpHierarchyPopup");
    }
    if (ImGui::BeginPopup("OpHierarchyPopup")) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Flat End Mill");
        if (ImGui::Selectable("  Slotting & Keyway Cutting")) {
            strncpy(m_InputOperationType, "Slotting & Keyway Cutting", sizeof(m_InputOperationType) - 1);
        }
        if (ImGui::Selectable("  Pocketing")) {
            strncpy(m_InputOperationType, "Pocketing", sizeof(m_InputOperationType) - 1);
        }
        if (ImGui::Selectable("  Peripheral Milling")) {
            strncpy(m_InputOperationType, "Peripheral Milling", sizeof(m_InputOperationType) - 1);
        }
        if (ImGui::Selectable("  Facing##FlatEndMill")) {
            strncpy(m_InputOperationType, "Facing", sizeof(m_InputOperationType) - 1);
        }
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Twist Drill");
        if (ImGui::Selectable("  Through-Hole & Blind-Hole Drilling")) {
            strncpy(m_InputOperationType, "Through-Hole & Blind-Hole Drilling", sizeof(m_InputOperationType) - 1);
        }
        if (ImGui::Selectable("  Pre-Drilling")) {
            strncpy(m_InputOperationType, "Pre-Drilling", sizeof(m_InputOperationType) - 1);
        }
        if (ImGui::Selectable("  Peck Drilling")) {
            strncpy(m_InputOperationType, "Peck Drilling", sizeof(m_InputOperationType) - 1);
        }
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Indexable Turning Tool");
        if (ImGui::Selectable("  Rough Turning")) {
            strncpy(m_InputOperationType, "Rough Turning", sizeof(m_InputOperationType) - 1);
        }
        if (ImGui::Selectable("  Finish Turning")) {
            strncpy(m_InputOperationType, "Finish Turning", sizeof(m_InputOperationType) - 1);
        }
        if (ImGui::Selectable("  Facing##Turning")) {
            strncpy(m_InputOperationType, "Facing", sizeof(m_InputOperationType) - 1);
        }
        if (ImGui::Selectable("  Taper & Profile Turning")) {
            strncpy(m_InputOperationType, "Taper & Profile Turning", sizeof(m_InputOperationType) - 1);
        }
        ImGui::EndPopup();
    }
    ImGui::Spacing();

    // Field 9 — Tray Number
    ImGui::Text("Tray No:");
    ImGui::SetNextItemWidth(320);
    ImGui::InputText("##TrayNo", m_InputTrayNo, sizeof(m_InputTrayNo));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* btnLabel = (m_CurrentIndex + 1 >= m_TargetQuantity) ? "Save Tool" : "Next Tool";
    if (ImGui::Button(btnLabel, ImVec2(130, 32))) {
        std::string idStr(m_InputID);
        std::string nameStr(m_InputName);
        std::string toolLifeStr(m_InputToolLife);
        std::string compLifeStr(m_InputCompletedLife);
        std::string visLifeStr(m_InputRemainingVisual);
        std::string mlLifeStr(m_InputRemainingML);
        std::string usageStr(m_InputTotalUsageHours);
        std::string cncIdStr(m_InputCncMachineId);
        std::string cncNameStr(m_InputCncMachineName);
        std::string opStr(m_InputOperationType);
        std::string trayStr(m_InputTrayNo);

        Trim(idStr);
        Trim(nameStr);
        Trim(toolLifeStr);
        Trim(compLifeStr);
        Trim(visLifeStr);
        Trim(mlLifeStr);
        Trim(usageStr);
        Trim(cncIdStr);
        Trim(cncNameStr);
        Trim(opStr);
        Trim(trayStr);

        if (idStr.empty() || nameStr.empty()) {
            m_ShowEmptyFieldPopup = true;
            return;
        }

        if (IDAlreadyExists(idStr, "Tool")) {
            m_DuplicateIDValue = idStr;
            m_ShowDuplicateIDPopup = true;
            return;
        }

        // Numeric validations + "N/A" defaults.
        // Delegated to validation::NormalizeAndValidate so this desktop app and
        // the web backend enforce byte-identical rules and popup messages.
        {
            validation::RawFields raw;
            raw.id              = m_InputID;
            raw.name            = m_InputName;
            raw.toolLife        = toolLifeStr;
            raw.completedLife   = compLifeStr;
            raw.remainingVisual = visLifeStr;
            raw.remainingML     = mlLifeStr;
            raw.totalUsageHours = usageStr;
            raw.cncMachineId    = cncIdStr;
            raw.cncMachineName  = cncNameStr;
            raw.operationType   = opStr;
            raw.trayNo          = trayStr;

            QRObject obj;
            validation::Result vr = validation::NormalizeAndValidate("Tool", raw, obj);
            if (!vr.ok) {
                m_ValidationErrorMessage = vr.message;
                m_ShowValidationErrorPopup = true;
                return;
            }

            // Accept and create object
            m_UsedIDs.insert(obj.id);
            m_PendingObjects.push_back(obj);
        }

        m_CurrentIndex++;
        if (m_CurrentIndex >= m_TargetQuantity) {
            m_State = AppState::SizeSelection;
        } else {
            ResetToolInputBuffers();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(100, 32))) {
        m_State = AppState::ToolQuantity;
    }
}

// ═══════════════════════════════════════════════════════════════
//  Size Selection
// ═══════════════════════════════════════════════════════════════

void App::DrawSizeSelection() {
    ImGui::Text("Type: %s  |  Count: %d", m_CurrentType.c_str(), m_TargetQuantity);
    ImGui::Separator();
    ImGui::Text("Choose QR Size (applies to all items):");
    ImGui::Spacing();

    static int sizeChoice = 0;
    ImGui::RadioButton("1.  9 x 9 mm  |  Depth 0.8 mm", &sizeChoice, 0);
    ImGui::RadioButton("2.  7 x 7 mm  |  Depth 1.0 mm", &sizeChoice, 1);
    ImGui::RadioButton("3.  5 x 5 mm  |  Depth 1.2 mm", &sizeChoice, 2);
    ImGui::RadioButton("4.  3 x 3 mm  |  Depth 1.5 mm", &sizeChoice, 3);

    if (sizeChoice == 3) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1,1,0,1), "WARNING: 3mm QR codes may be hard to scan.");
    }

    ImGui::Spacing();
    if (ImGui::Button("Generate QRs and Preview", ImVec2(220, 36))) {
        float sz, dp;
        if      (sizeChoice == 0) { sz = 9.f; dp = 0.8f; }
        else if (sizeChoice == 1) { sz = 7.f; dp = 1.0f; }
        else if (sizeChoice == 2) { sz = 5.f; dp = 1.2f; }
        else                      { sz = 3.f; dp = 1.5f; }

        for (auto& obj : m_PendingObjects) {
            obj.sizeMM  = sz;
            obj.depthMM = dp;
        }
        GenerateAllQRs();
        m_PreviewIndex   = 0;
        m_PreviewUpdated = true;
        m_State = AppState::Preview;
    }
}

// ═══════════════════════════════════════════════════════════════
//  Preview panel
// ═══════════════════════════════════════════════════════════════

void App::DrawPreviewMenu() {
    if (m_PendingObjects.empty()) return;

    const QRObject& cur = m_PendingObjects[m_PreviewIndex];
    fs::path stlPath = GetSTLPath(cur);
    fs::path pngPath = GetPNGPath(cur);

    ImGui::Text("--------------------------------------------------");
    ImGui::Text("               3D QR CODE PREVIEW");
    ImGui::Text("--------------------------------------------------");
    ImGui::Spacing();
    ImGui::Text("  QR %d / %d", m_PreviewIndex + 1, m_TargetQuantity);
    ImGui::Separator();

    ImGui::Text("Type:          %s", cur.type.c_str());
    ImGui::Text("ID:            %s", cur.id.c_str());
    ImGui::Text("Name:          %s", cur.name.c_str());
    if (cur.type == "Holder") {
        ImGui::Text("Calib. Status: %s", cur.calibrationStatus.c_str());
        ImGui::Text("Accum. Life:   %s", cur.accumulatedLife.c_str());
        ImGui::Text("Serial/Batch:  %s", cur.serialBatchNumber.c_str());
    } else {
        ImGui::Text("Tool Life:     %.2f m", cur.toolLifeMeters);
        ImGui::Text("Completed Life:%.2f m", cur.completedLifeMeters);
        ImGui::Text("Rem. (Visual): %.2f m", cur.remainingLifeVisualMeters);
        ImGui::Text("Rem. (ML):     %.2f m", cur.remainingLifeMLMeters);
        ImGui::Text("Total Usage:   %.2f hrs", cur.totalUsageHours);
        ImGui::Text("CNC ID:        %s", cur.cncMachineId.c_str());
        ImGui::Text("CNC Name:      %s", cur.cncMachineName.c_str());
        ImGui::Text("Operation:     %s", cur.operationType.c_str());
        ImGui::Text("Tray No:       %s", cur.trayNo.c_str());
    }
    ImGui::Text("Size:          %.1f x %.1f mm", cur.sizeMM, cur.sizeMM);
    ImGui::Text("Depth:         %.1f mm", cur.depthMM);
    ImGui::Spacing();

    // STL status — filesystem is authoritative
    bool stlOnDisk = fs::exists(stlPath);
    bool pngOnDisk = fs::exists(pngPath);

    ImGui::Text("STL: %s", stlPath.string().c_str());
    if (stlOnDisk)
        ImGui::TextColored(ImVec4(0.2f,1,0.4f,1), "  Status: Generated \xE2\x9C\x93");
    else
        ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "  Status: Not generated");

    ImGui::Text("PNG: %s", pngPath.string().c_str());
    if (pngOnDisk)
        ImGui::TextColored(ImVec4(0.2f,1,0.4f,1), "  Status: Generated \xE2\x9C\x93");
    else
        ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "  Status: Not generated");

    ImGui::Separator();
    ImGui::Spacing();

    // Prev / Next
    const bool disablePrev = (m_PreviewIndex <= 0);
    if (disablePrev) ImGui::BeginDisabled();
    if (ImGui::Button("< Previous")) { m_PreviewIndex--; m_PreviewUpdated = true; }
    if (disablePrev) ImGui::EndDisabled();

    ImGui::SameLine();

    const bool disableNext = (m_PreviewIndex >= m_TargetQuantity - 1);
    if (disableNext) ImGui::BeginDisabled();
    if (ImGui::Button("Next >")) { m_PreviewIndex++; m_PreviewUpdated = true; }
    if (disableNext) ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Export Current STL",  ImVec2(200, 30))) TryExportCurrentSTL();
    if (ImGui::Button("Export Current PNG",  ImVec2(200, 30))) TryExportCurrentPNG();
    if (ImGui::Button("Generate All STL",    ImVec2(200, 30))) TryExportAllSTLs();
    if (ImGui::Button("Generate All PNG",    ImVec2(200, 30))) TryExportAllPNGs();

    ImGui::Spacing();
    if (ImGui::Button("Back to Main Menu")) {
        m_State = AppState::MainMenu;
        m_PendingObjects.clear();
        m_UsedIDs.clear();
        m_PreviewIndex = 0;
    }
}

// ═══════════════════════════════════════════════════════════════
//  QR generation  (QR matrix is shared → PNG + 3D mesh)
// ═══════════════════════════════════════════════════════════════

void App::GenerateAllQRs() {
    std::cout << "[QR] Generating " << m_PendingObjects.size() << " object(s)\n";
    for (auto& obj : m_PendingObjects) {
        // Shared engine step: payload -> QR matrix -> 3D mesh -> canonical paths.
        // Identical to the web backend's engine invocation.
        std::string error;
        if (!engine::GenerateObject(obj, error)) {
            std::cerr << "[QR ERROR] " << obj.id << ": " << error << "\n";
            m_FailureMessage = "QR Generation Failed\n\n" + error;
            m_ShowFailurePopup = true;
            continue;
        }

        std::cout << "[QR] " << obj.id << " → " << obj.mesh.triangles.size()
                  << " triangles, matrix " << obj.qrMatrix.size() << "x"
                  << (obj.qrMatrix.empty() ? 0 : obj.qrMatrix[0].size()) << "\n";
    }
    std::cout << "[QR] Done.\n";
}

// ═══════════════════════════════════════════════════════════════
//  STL export helpers
// ═══════════════════════════════════════════════════════════════

bool App::DoExportSTL(QRObject& obj) {
    fs::path outPath = GetSTLPath(obj);

    std::cout << "[STL] Exporting " << obj.id << " → " << outPath.string() << "\n";

    // Shared engine step — creates the directory, writes the binary STL and
    // verifies the file really landed (App::DoExportSTL in the original).
    engine::ExportResult res = engine::ExportSTL(obj, outPath);
    if (!res.ok) {
        std::cerr << "[STL ERROR] " << res.message << "\n";
        m_FailureMessage = res.message;
        m_ShowFailurePopup = true;
        return false;
    }

    obj.stlGenerated = true;
    obj.stlPath      = outPath;
    std::cout << "[STL] OK – " << res.bytes << " bytes → "
              << fs::absolute(outPath).string() << "\n";
    m_ExportSuccessCount++;
    return true;
}

// ═══════════════════════════════════════════════════════════════
//  PNG export helpers
// ═══════════════════════════════════════════════════════════════

bool App::DoExportPNG(QRObject& obj) {
    if (obj.qrMatrix.empty()) {
        m_FailureMessage = "PNG Export Failed\n\nNo QR matrix available.\nGenerate the QR first.";
        m_ShowFailurePopup = true;
        return false;
    }

    fs::path outPath = GetPNGPath(obj);
    std::cout << "[PNG] Exporting " << obj.id << " → " << outPath.string() << "\n";

    // Shared engine step — same qrMatrix that built the 3D mesh, written with
    // the desktop app's fixed scale=10 px/module and quietZone=4.
    engine::ExportResult res = engine::ExportPNG(obj, outPath, 10, 4);
    if (!res.ok) {
        std::cerr << "[PNG ERROR] " << res.message << "\n";
        m_FailureMessage = res.message;
        m_ShowFailurePopup = true;
        return false;
    }

    obj.pngGenerated = true;
    obj.pngPath      = outPath;
    std::cout << "[PNG] OK – " << res.bytes << " bytes → "
              << fs::absolute(outPath).string() << "\n";
    m_ExportSuccessCount++;
    return true;
}

// ─────────────────────────────────────────────────────────────
//  Single-item export (checks filesystem before overwrite popup)
// ─────────────────────────────────────────────────────────────

void App::TryExportCurrentSTL() {
    if (m_PreviewIndex < 0 || m_PreviewIndex >= (int)m_PendingObjects.size()) return;
    QRObject& obj    = m_PendingObjects[m_PreviewIndex];
    fs::path  path   = GetSTLPath(obj);

    std::cout << "[STL] exists check: " << path.string()
              << " → " << (fs::exists(path) ? "YES" : "NO") << "\n";

    if (fs::exists(path)) {
        // Show overwrite confirmation — overwrite is ONLY shown when file really exists
        m_OverwritePath    = path.string();
        m_OverwriteIsPNG   = false;
        m_ExportQueue      = { (size_t)m_PreviewIndex };
        m_ExportQueueIdx   = 0;
        m_ExportSuccessCount = 0;
        m_ShowOverwritePopup = true;
    } else {
        m_ExportSuccessCount = 0;
        if (DoExportSTL(obj) && !m_ShowFailurePopup) {
            m_SuccessMessage = "STL Export Successful\n\nType: " + obj.type +
                               "\nID:   " + obj.id +
                               "\n\nFile:\n" + fs::absolute(path).string();
            m_ShowSuccessPopup = true;
        }
    }
}

void App::TryExportCurrentPNG() {
    if (m_PreviewIndex < 0 || m_PreviewIndex >= (int)m_PendingObjects.size()) return;
    QRObject& obj  = m_PendingObjects[m_PreviewIndex];
    fs::path  path = GetPNGPath(obj);

    std::cout << "[PNG] exists check: " << path.string()
              << " → " << (fs::exists(path) ? "YES" : "NO") << "\n";

    if (fs::exists(path)) {
        m_OverwritePath    = path.string();
        m_OverwriteIsPNG   = true;
        m_ExportQueue      = { (size_t)m_PreviewIndex };
        m_ExportQueueIdx   = 0;
        m_ExportSuccessCount = 0;
        m_ShowOverwritePopup = true;
    } else {
        m_ExportSuccessCount = 0;
        if (DoExportPNG(obj) && !m_ShowFailurePopup) {
            m_SuccessMessage = "PNG Export Successful\n\nType: " + obj.type +
                               "\nID:   " + obj.id +
                               "\n\nFile:\n" + fs::absolute(path).string();
            m_ShowSuccessPopup = true;
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  Batch export
// ─────────────────────────────────────────────────────────────

void App::TryExportAllSTLs() {
    m_ExportQueue.clear();
    for (size_t i = 0; i < m_PendingObjects.size(); ++i) m_ExportQueue.push_back(i);
    m_ExportQueueIdx   = 0;
    m_ExportSuccessCount = 0;
    m_ExportIsPNG      = false;
    ExportNextInQueue();
}

void App::TryExportAllPNGs() {
    m_ExportQueue.clear();
    for (size_t i = 0; i < m_PendingObjects.size(); ++i) m_ExportQueue.push_back(i);
    m_ExportQueueIdx   = 0;
    m_ExportSuccessCount = 0;
    m_ExportIsPNG      = true;
    ExportNextInQueue();
}

void App::ExportNextInQueue() {
    while (m_ExportQueueIdx < m_ExportQueue.size()) {
        size_t   idx  = m_ExportQueue[m_ExportQueueIdx];
        QRObject& obj = m_PendingObjects[idx];

        fs::path path = m_ExportIsPNG ? GetPNGPath(obj) : GetSTLPath(obj);

        if (fs::exists(path)) {
            m_OverwritePath    = path.string();
            m_OverwriteIsPNG   = m_ExportIsPNG;
            m_ShowOverwritePopup = true;
            return; // pause for user decision
        }

        bool ok = m_ExportIsPNG ? DoExportPNG(obj) : DoExportSTL(obj);
        m_ExportQueueIdx++;
        if (!ok && m_ShowFailurePopup) return;
    }

    // All done
    if (!m_ShowFailurePopup && m_ExportSuccessCount > 0) {
        if (m_ExportSuccessCount == 1) {
            size_t idx = m_ExportQueue[0];
            const QRObject& obj = m_PendingObjects[idx];
            fs::path p = m_ExportIsPNG ? GetPNGPath(obj) : GetSTLPath(obj);
            m_SuccessMessage = std::string(m_ExportIsPNG ? "PNG" : "STL") +
                               " Export Successful\n\nType: " + obj.type +
                               "\nID:   " + obj.id +
                               "\n\nFile:\n" + fs::absolute(p).string();
        } else {
            m_SuccessMessage = std::to_string(m_ExportSuccessCount) + " " +
                               std::string(m_ExportIsPNG ? "PNG" : "STL") +
                               " files generated successfully.";
        }
        m_ShowSuccessPopup = true;
    }
}

// ═══════════════════════════════════════════════════════════════
//  Popups
// ═══════════════════════════════════════════════════════════════

void App::DrawPopups() {

    // ── Duplicate ID ─────────────────────────────────────────
    if (m_ShowDuplicateIDPopup) {
        ImGui::OpenPopup("Duplicate ID");
        m_ShowDuplicateIDPopup = false;
    }
    if (ImGui::BeginPopupModal("Duplicate ID", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "Duplicate ID");
        ImGui::Separator();
        ImGui::Text(
            "The ID \"%s\" is already in use.\n\n"
            "A file with this ID already exists on disk.\n\n"
            "Please enter a different ID.",
            m_DuplicateIDValue.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120,0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Empty field ───────────────────────────────────────────
    if (m_ShowEmptyFieldPopup) { ImGui::OpenPopup("Empty Field"); m_ShowEmptyFieldPopup = false; }
    if (ImGui::BeginPopupModal("Empty Field", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "ERROR");
        ImGui::Separator();
        ImGui::Text("ID and Name cannot be empty.\n\nPlease enter valid information.");
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120,0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Success ───────────────────────────────────────────────
    if (m_ShowSuccessPopup) { ImGui::OpenPopup("Export Success"); m_ShowSuccessPopup = false; }
    if (ImGui::BeginPopupModal("Export Success", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.2f,1,0.4f,1), "SUCCESS");
        ImGui::Separator();
        ImGui::Text("%s", m_SuccessMessage.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120,0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── Failure ───────────────────────────────────────────────
    if (m_ShowFailurePopup) { ImGui::OpenPopup("Export Failed"); m_ShowFailurePopup = false; }
    if (ImGui::BeginPopupModal("Export Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "EXPORT FAILED");
        ImGui::Separator();
        ImGui::Text("%s", m_FailureMessage.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120,0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── File Already Exists → Overwrite? ─────────────────────
    if (m_ShowOverwritePopup) { ImGui::OpenPopup("File Already Exists"); m_ShowOverwritePopup = false; }
    if (ImGui::BeginPopupModal("File Already Exists", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1,0.85f,0,1), "FILE ALREADY EXISTS");
        ImGui::Separator();
        ImGui::Text("%s\n\nDo you want to overwrite it?", m_OverwritePath.c_str());
        ImGui::Spacing();

        if (ImGui::Button("Overwrite", ImVec2(110,0))) {
            // Delete then re-export
            std::error_code ec;
            fs::remove(m_OverwritePath, ec);

            if (m_ExportQueueIdx < m_ExportQueue.size()) {
                size_t idx = m_ExportQueue[m_ExportQueueIdx];
                bool ok = m_OverwriteIsPNG ? DoExportPNG(m_PendingObjects[idx])
                                           : DoExportSTL(m_PendingObjects[idx]);
                m_ExportQueueIdx++;
                ImGui::CloseCurrentPopup();
                if (ok) ExportNextInQueue();
            } else {
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110,0))) {
            m_ExportQueueIdx++; // skip this item
            ImGui::CloseCurrentPopup();
            ExportNextInQueue();
        }
        ImGui::EndPopup();
    }

    // ── Validation Error ──────────────────────────────────────
    if (m_ShowValidationErrorPopup) {
        ImGui::OpenPopup("Validation Error");
        m_ShowValidationErrorPopup = false;
    }
    if (ImGui::BeginPopupModal("Validation Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "VALIDATION ERROR");
        ImGui::Separator();
        ImGui::Text("%s", m_ValidationErrorMessage.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

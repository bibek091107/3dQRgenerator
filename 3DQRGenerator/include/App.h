#pragma once

#include "QRObject.h"
#include "Mesh.h"
#include "Camera.h"
#include "Renderer.h"
#include <vector>
#include <string>
#include <unordered_set>
#include <filesystem>

enum class AppState {
    MainMenu,
    HolderQuantity,
    HolderDetails,
    ToolQuantity,
    ToolDetails,
    SizeSelection,
    Preview
};

class App {
public:
    App();
    ~App();
    void Run();

private:
    // ── Window ─────────────────────────────────
    struct GLFWwindow* m_Window;

    // ── State machine ──────────────────────────
    AppState    m_State;
    std::string m_CurrentType;   // "Holder" or "Tool"
    int         m_TargetQuantity = 0;
    int         m_CurrentIndex   = 0;
    int         m_PreviewIndex   = 0;

    // ── Data ───────────────────────────────────
    std::vector<QRObject>             m_PendingObjects;
    std::unordered_set<std::string>   m_UsedIDs;   // session-only fast-reject

    // ── Input buffers ──────────────────────────
    // Shared / Identification
    char m_InputID               [128] = "";
    char m_InputName             [256] = "";
    char m_QuantityStr           [16]  = "";

    // Holder specific input buffers
    char m_InputCalibrationStatus[128] = "";
    char m_InputAccumulatedLife  [128] = "";
    char m_InputSerialBatchNumber[128] = "";

    // Tool specific input buffers
    char m_InputToolLife         [128] = "";
    char m_InputCompletedLife    [128] = "";
    char m_InputRemainingVisual  [128] = "";
    char m_InputRemainingML      [128] = "";
    char m_InputTotalUsageHours  [128] = "";
    char m_InputCncMachineId     [128] = "";
    char m_InputCncMachineName   [128] = "";
    char m_InputOperationType    [256] = "";
    char m_InputTrayNo           [128] = "";

    // ── Rendering ──────────────────────────────
    Camera   m_Camera;
    Renderer m_Renderer;
    bool     m_PreviewUpdated = false;

    // ── Lifecycle ──────────────────────────────
    void InitWindow();
    void InitImGui();
    void Cleanup();

    // ── UI ─────────────────────────────────────
    void RenderUI();
    void DrawMainMenu();
    void DrawQuantityMenu();
    void DrawHolderDetailsMenu();
    void DrawToolDetailsMenu();
    void DrawSizeSelection();
    void DrawPreviewMenu();
    void Draw2DQRScannerHUD();
    void DrawPopups();

    void ResetHolderInputBuffers();
    void ResetToolInputBuffers();

    // ── Path helpers (filesystem source of truth) ──
    std::filesystem::path GetSTLPath(const QRObject& obj) const;
    std::filesystem::path GetPNGPath(const QRObject& obj) const;

    // Returns true if ANY output file already exists for this id+type
    // (checks BOTH memory set AND the actual filesystem)
    bool IDAlreadyExists(const std::string& id, const std::string& type) const;

    // ── QR generation ──────────────────────────
    void GenerateAllQRs();

    // ── Export ─────────────────────────────────
    void TryExportCurrentSTL();
    void TryExportAllSTLs();
    void TryExportCurrentPNG();
    void TryExportAllPNGs();

    // Inner export helpers — return false on failure
    bool DoExportSTL(QRObject& obj);
    bool DoExportPNG(QRObject& obj);

    // Batch export queue
    std::vector<size_t> m_ExportQueue;
    size_t m_ExportQueueIdx      = 0;
    int    m_ExportSuccessCount  = 0;
    bool   m_ExportIsPNG         = false; // which export type is queued

    void ExportNextInQueue();

    // ── Popup state ────────────────────────────
    bool        m_ShowDuplicateIDPopup     = false;
    bool        m_ShowEmptyFieldPopup      = false;
    bool        m_ShowValidationErrorPopup = false;
    bool        m_ShowSuccessPopup         = false;
    bool        m_ShowFailurePopup         = false;
    bool        m_ShowOverwritePopup       = false;

    std::string m_DuplicateIDValue;
    std::string m_ValidationErrorMessage;
    std::string m_SuccessMessage;
    std::string m_FailureMessage;
    std::string m_OverwritePath;     // full path displayed in popup
    bool        m_OverwriteIsPNG     = false; // which type the pending overwrite is for
};

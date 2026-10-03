"use client";

/**
 * Control-panel state machine, a direct port of App::RenderUI() plus the
 * popup/overwrite behaviour of App::DrawPopups().
 *
 * The desktop app kept everything in memory (m_PendingObjects / m_UsedIDs); the
 * web app keeps the same in-batch semantics and additionally mirrors the list in
 * PostgreSQL so exports, downloads and 3D previews survive a page reload.
 */

import { useCallback, useEffect, useRef, useState } from "react";

import { ApiError, api } from "@/lib/api";
import { emptyFields, type FieldKey } from "@/lib/fields";
import type { ExportKind, ObjectStatus, QrType, QRObject } from "@/lib/types";
import { HolderDetails } from "@/components/HolderDetails";
import { MainMenu } from "@/components/MainMenu";
import { PopupModal, type Popup } from "@/components/PopupModal";
import { PreviewScreen } from "@/components/PreviewScreen";
import { QRViewer3D } from "@/components/QRViewer3D";
import { QuantityScreen } from "@/components/QuantityScreen";
import { SizeSelection } from "@/components/SizeSelection";
import { ToolDetails } from "@/components/ToolDetails";

type Screen = "main" | "quantity" | "holderDetails" | "toolDetails" | "size" | "preview";

type Queue = {
  kind: ExportKind;
  indices: number[];
  idx: number;
  successCount: number;
};

export default function ControlPanel() {
  const [screen, setScreen] = useState<Screen>("main");
  const [type, setType] = useState<QrType>("Holder");
  const [quantityText, setQuantityText] = useState("");
  const [targetQuantity, setTargetQuantity] = useState(0);

  const [batchId, setBatchId] = useState<string | null>(null);
  const [fields, setFields] = useState<Record<FieldKey, string>>(emptyFields("Holder"));

  const [sizeChoice, setSizeChoice] = useState(0);
  const [objects, setObjects] = useState<QRObject[]>([]);
  const [previewIndex, setPreviewIndex] = useState(0);

  const [popup, setPopup] = useState<Popup | null>(null);
  const [busy, setBusy] = useState(false);
  const [notice, setNotice] = useState<string | null>(null);
  const [exited, setExited] = useState(false);

  const queueRef = useRef<Queue | null>(null);

  // ── Backend banner ───────────────────────────────────────────────
  const [health, setHealth] = useState<{ database: boolean; engine: boolean } | null>(null);
  useEffect(() => {
    api
      .health()
      .then((h) => setHealth({ database: h.database.ok, engine: h.engine.ok }))
      .catch(() => setHealth({ database: false, engine: false }));
  }, []);

  // ── Popup helpers, matching the desktop popup titles/bodies ──────
  const showFailure = useCallback((message: string) => {
    setPopup({ kind: "failure", message });
  }, []);

  const reportError = useCallback(
    (e: unknown) => {
      if (e instanceof ApiError) {
        switch (e.code) {
          case "empty_field":
            setPopup({
              kind: "emptyField",
              message: "ID and Name cannot be empty.\n\nPlease enter valid information.",
            });
            return;
          case "duplicate_id":
            setNotice(null);
            setPopup({
              kind: "duplicateId",
              message: e.detail.message ?? `The ID "${e.detail.duplicateId ?? ""}" is already in use.`,
            });
            return;
          case "validation_error":
            setPopup({ kind: "validationError", message: e.message });
            return;
          default:
            showFailure(e.message);
            return;
        }
      }
      showFailure(e instanceof Error ? e.message : String(e));
    },
    [showFailure],
  );

  // ── Preview status (the filesystem is the source of truth) ───────
  const [status, setStatus] = useState<{ objectId: string; value: ObjectStatus } | null>(null);

  const refreshStatus = useCallback(async (objectId: string) => {
    const value = await api.objectStatus(objectId);
    setStatus({ objectId, value });
  }, []);

  useEffect(() => {
    const cur = objects[previewIndex];
    if (screen !== "preview" || !cur) return;
    let cancelled = false;
    (async () => {
      try {
        const value = await api.objectStatus(cur.id);
        if (!cancelled) setStatus({ objectId: cur.id, value });
      } catch {
        // Leave the previous status in place; the panel shows the last known state.
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [screen, previewIndex, objects]);

  // ── Main menu ────────────────────────────────────────────────────
  const startBatch = (next: QrType) => {
    setType(next);
    setQuantityText("");
    setScreen("quantity");
  };

  const toMainMenu = () => {
    setScreen("main");
    setObjects([]);
    setBatchId(null);
    setPreviewIndex(0);
    setStatus(null);
    setNotice(null);
  };

  // ── Quantity ─────────────────────────────────────────────────────
  const confirmQuantity = async () => {
    const qty = parseInt(quantityText, 10);
    // The desktop app only advanced when qty > 0.
    if (!(qty > 0)) return;

    setBusy(true);
    setNotice(null);
    try {
      // Re-entering the flow discards the pending objects, like the desktop
      // app clearing m_PendingObjects / m_UsedIDs.
      if (batchId) await api.deleteBatch(batchId).catch(() => undefined);
      const batch = await api.createBatch(type, qty);
      setBatchId(batch.id);
      setTargetQuantity(qty);
      setObjects([]);
      setFields(emptyFields(type));
      setScreen(type === "Holder" ? "holderDetails" : "toolDetails");
    } catch (e) {
      reportError(e);
    } finally {
      setBusy(false);
    }
  };

  // ── Save one object ──────────────────────────────────────────────
  const saveObject = async () => {
    if (!batchId) return;
    setBusy(true);
    setNotice(`Registering ${type}...`);
    try {
      const created = await api.createObject(batchId, type, fields);
      const next = [...objects, created];
      setObjects(next);
      setFields(emptyFields(type));
      setNotice(`${type} registered successfully.`);
      if (next.length >= targetQuantity) {
        setScreen("size");
      }
    } catch (e) {
      setNotice(null);
      reportError(e);
    } finally {
      setBusy(false);
    }
  };

  // ── Generate ─────────────────────────────────────────────────────
  const generate = async () => {
    if (!batchId) return;
    setBusy(true);
    setNotice(null);
    try {
      const detail = await api.generate(batchId, sizeChoice);
      setObjects(detail.objects ?? []);
      setPreviewIndex(0);
      setScreen("preview");
    } catch (e) {
      reportError(e);
    } finally {
      setBusy(false);
    }
  };

  // ── Export queue (App::TryExport*STLs / ExportNextInQueue) ───────
  const runQueue = useCallback(
    async (q: Queue, overwriteThis: boolean) => {
      setBusy(true);
      try {
        let successCount = q.successCount;
        const downloaded: string[] = [];
        for (let i = q.idx; i < q.indices.length; i++) {
          const obj = objects[q.indices[i]];
          if (!obj) continue;

          if (!overwriteThis) {
            // The overwrite prompt is raised ONLY when the file really exists.
            const st = await api.objectStatus(obj.id);
            const target = q.kind === "stl" ? st.stl : st.png;
            if (target.exists) {
              queueRef.current = { ...q, idx: i, successCount };
              setPopup({ kind: "overwrite", path: target.path, message: target.path });
              return; // pause for the user's decision, like ExportNextInQueue()
            }
          }

          try {
            await api.exportObject(obj.id, q.kind, overwriteThis);
          } catch (e) {
            queueRef.current = null;
            reportError(e);
            return;
          }

          // The engine's copy lives in the shared server output folder; pull
          // the same byte-exact file onto the operator's own device as well.
          let savedAs: string | null = null;
          try {
            savedAs = await api.downloadToDevice(obj.id, q.kind);
          } catch {
            // A failed device download must not abort the remaining queue;
            // the server-side export already succeeded.
          }

          successCount++;
          if (savedAs) downloaded.push(savedAs);
        }

        queueRef.current = null;

        // The panel reads status from disk, so refresh what is on screen.
        const previewed = objects[previewIndex];
        if (previewed) await refreshStatus(previewed.id).catch(() => undefined);

        // All done — the desktop app only reported success if nothing failed.
        if (successCount > 0) {
          const label = q.kind === "stl" ? "STL" : "PNG";

          if (successCount === 1) {
            const obj = objects[q.indices[0]];
            const saved = downloaded.length === 1 ? `\n\nSaved to this device:\n${downloaded[0]}` : "";
            try {
              const st = await api.objectStatus(obj.id);
              const path = q.kind === "stl" ? st.stl.path : st.png.path;
              setPopup({
                kind: "success",
                message: `${label} Export Successful\n\nType: ${obj.type}\nID:   ${obj.qrId}\n\nFile:\n${path}${saved}`,
              });
            } catch {
              setPopup({ kind: "success", message: `${label} Export Successful${saved}` });
            }
          } else {
            const saved =
              downloaded.length > 0
                ? `\n\nSaved ${downloaded.length} file(s) to this device.`
                : "";
            setPopup({
              kind: "success",
              message: `${successCount} ${label} files generated successfully.${saved}`,
            });
          }
        }
      } finally {
        setBusy(false);
      }
    },
    [objects, previewIndex, refreshStatus, reportError],
  );

  const exportCurrent = (kind: ExportKind) => {
    if (previewIndex < 0 || previewIndex >= objects.length) return;
    void runQueue({ kind, indices: [previewIndex], idx: 0, successCount: 0 }, false);
  };

  const exportAll = (kind: ExportKind) => {
    void runQueue(
      { kind, indices: objects.map((_, i) => i), idx: 0, successCount: 0 },
      false,
    );
  };

  const onOverwrite = () => {
    const q = queueRef.current;
    setPopup(null);
    if (q) void runQueue(q, true);
  };

  const onCancel = () => {
    const q = queueRef.current;
    setPopup(null);
    if (q) void runQueue({ ...q, idx: q.idx + 1 }, false);
  };

  // ── Render ───────────────────────────────────────────────────────
  if (exited) {
    return (
      <div className="flex h-screen items-center justify-center">
        <div className="rounded-lg border border-[#3a3a46] bg-[#23232b] px-8 py-6 text-center">
          <div className="text-lg font-bold tracking-widest text-[#e6e8ef]">
            3D QR CODE GENERATOR
          </div>
          <div className="mt-2 text-sm text-[#9aa0ae]">Application closed.</div>
          <button
            type="button"
            onClick={() => setExited(false)}
            className="mt-4 rounded border border-[#3d3d49] bg-[#2a2a33] px-4 py-1.5 text-sm text-[#dfe2ea] hover:bg-[#33333e]"
          >
            Relaunch
          </button>
        </div>
      </div>
    );
  }

  const detailsProps = {
    index: objects.length,
    total: targetQuantity,
    values: fields as unknown as Record<string, string>,
    onChange: (key: string, next: string) =>
      setFields((prev) => ({ ...prev, [key as FieldKey]: next })),
    onSave: saveObject,
    onBack: () => setScreen("quantity"),
    busy,
  };

  return (
    <div className="relative min-h-screen w-full">
      {/* 3D viewport — the native OpenGL scene behind the ImGui panel. */}
      {screen === "preview" && objects[previewIndex] ? (
        <div className="fixed inset-0">
          <QRViewer3D
            objectId={objects[previewIndex].id}
            sizeMM={objects[previewIndex].sizeMM}
          />
        </div>
      ) : null}

      {/* Control panel — ImGui's always-auto-resize "Control Panel" window.
          The wrapper spans the viewport so the panel sits on the left of the
          3D scene; it must ignore pointer events, otherwise it covers the
          canvas and OrbitControls never receives a drag. The panel column
          re-enables them. */}
      <div className="pointer-events-none relative z-10 flex min-h-screen items-start gap-4 p-5">
        <div className="pointer-events-auto max-h-[calc(100vh-2.5rem)] overflow-y-auto">
          {screen === "main" ? <MainMenu onSelect={startBatch} onExit={() => setExited(true)} /> : null}

          {screen === "quantity" ? (
            <QuantityScreen
              type={type}
              value={quantityText}
              onChange={setQuantityText}
              onNext={confirmQuantity}
              onBack={toMainMenu}
            />
          ) : null}

          {screen === "holderDetails" ? <HolderDetails {...detailsProps} /> : null}
          {screen === "toolDetails" ? <ToolDetails {...detailsProps} /> : null}

          {screen === "size" ? (
            <SizeSelection
              type={type}
              count={targetQuantity}
              sizeChoice={sizeChoice}
              onSizeChoice={setSizeChoice}
              onGenerate={generate}
              busy={busy}
            />
          ) : null}

          {screen === "preview" ? (
            <PreviewScreen
              objects={objects}
              index={previewIndex}
              status={status?.objectId === objects[previewIndex]?.id ? status.value : null}
              onPrev={() => setPreviewIndex((i) => Math.max(0, i - 1))}
              onNext={() => setPreviewIndex((i) => Math.min(objects.length - 1, i + 1))}
              onExportCurrent={exportCurrent}
              onExportAll={exportAll}
              onBack={toMainMenu}
              busy={busy}
            />
          ) : null}
        </div>

        <div className="rounded bg-black/40 px-2 py-1 font-mono text-[10px] text-[#8b909d]">
          {health
            ? health.engine && health.database
              ? "engine + database ready"
              : `engine ${health.engine ? "ok" : "DOWN"} / db ${health.database ? "ok" : "DOWN"}`
            : "checking backend…"}
        </div>
      </div>

      {notice ? (
        <div className="fixed right-4 bottom-4 rounded bg-black/70 px-3 py-2 text-xs text-[#9aa0ae]">
          {notice}
        </div>
      ) : null}

      <PopupModal
        popup={popup}
        onClose={() => setPopup(null)}
        onOverwrite={onOverwrite}
        onCancel={onCancel}
      />
    </div>
  );
}
"use client";

/** Modal reproducing the ImGui popup modals from App::DrawPopups(). */

import type { ReactNode } from "react";
import { Button } from "./ui";

export type PopupKind =
  | "duplicateId"
  | "emptyField"
  | "success"
  | "failure"
  | "overwrite"
  | "validationError";

export type Popup = {
  kind: PopupKind;
  /** Pre-composed body text, byte-identical to the desktop ImGui::Text calls. */
  message: string;
  /** Overwrite popup path. */
  path?: string;
};

export function PopupModal({
  popup,
  onClose,
  onOverwrite,
  onCancel,
}: {
  popup: Popup | null;
  onClose: () => void;
  onOverwrite: () => void;
  onCancel: () => void;
}) {
  if (!popup) return null;

  const header: Record<PopupKind, { label: string; className: string }> = {
    duplicateId: { label: "Duplicate ID", className: "text-[#ff4d4d]" },
    emptyField: { label: "ERROR", className: "text-[#ff4d4d]" },
    success: { label: "SUCCESS", className: "text-[#33ff66]" },
    failure: { label: "EXPORT FAILED", className: "text-[#ff4d4d]" },
    overwrite: { label: "FILE ALREADY EXISTS", className: "text-[#ffd900]" },
    validationError: { label: "VALIDATION ERROR", className: "text-[#ff4d4d]" },
  };

  const isOverwrite = popup.kind === "overwrite";

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-4"
      role="dialog"
      aria-modal="true"
      onClick={(e) => {
        // Only Overwrite / Cancel dismiss the overwrite dialog, like ImGui.
        if (e.target === e.currentTarget && !isOverwrite) onClose();
      }}
    >
      <div className="w-full max-w-lg rounded-lg border border-[#3a3a46] bg-[#23232b] p-4 shadow-2xl">
        <h2 className={`text-base font-bold tracking-wide ${header[popup.kind].className}`}>
          {header[popup.kind].label}
        </h2>
        <div className="my-3 border-t border-[#3a3a46]" />
        <BodyText kind={popup.kind} text={popup.message} />

        <div className="mt-4 flex gap-2">
          {isOverwrite ? (
            <>
              <Button variant="primary" onClick={onOverwrite}>
                Overwrite
              </Button>
              <Button onClick={onCancel}>Cancel</Button>
            </>
          ) : (
            <Button variant="primary" onClick={onClose}>
              OK
            </Button>
          )}
        </div>
      </div>
    </div>
  );
}

function BodyText({ kind, text }: { kind: PopupKind; text: string }) {
  if (kind === "duplicateId") {
    // "The ID "%s" is already in use." + explanation paragraph
    const quoteMatch = /^The ID "([^"]*)" is already in use\./.exec(text);
    const id = quoteMatch?.[1] ?? "";
    return (
      <div className="space-y-2 whitespace-pre-wrap text-sm text-[#e2e4ea]">
        <div>{`The ID "${id}" is already in use.`}</div>
        <div>A file with this ID already exists on disk.</div>
        <div>Please enter a different ID.</div>
      </div>
    );
  }
  if (kind === "overwrite") {
    // ImGui::Text("%s\n\nDo you want to overwrite it?", m_OverwritePath.c_str())
    return (
      <div className="space-y-2 whitespace-pre-wrap text-sm text-[#e2e4ea]">
        <div className="font-mono break-all">{text}</div>
        <div>Do you want to overwrite it?</div>
      </div>
    );
  }
  return <pre className="font-sans text-sm whitespace-pre-wrap text-[#e2e4ea]">{text}</pre>;
}

export function StatusFooter({ children }: { children: ReactNode }) {
  return (
    <div className="flex items-center gap-2 text-xs text-[#8b909d]">
      <span className="h-1.5 w-1.5 rounded-full bg-[#3f9a5a]" />
      {children}
    </div>
  );
}
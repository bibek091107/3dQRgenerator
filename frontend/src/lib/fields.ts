/**
 * Field descriptors mirroring the desktop app's ImGui input widgets.
 *
 * `maxLength` is sizeof(buffer) - 1 from App.h, so the web inputs hold exactly
 * as many characters as the original char buffers did.
 */

import type { QrType } from "./types";

export type FieldKey =
  | "id"
  | "name"
  | "calibrationStatus"
  | "accumulatedLife"
  | "serialBatchNumber"
  | "toolLife"
  | "completedLife"
  | "remainingVisual"
  | "remainingML"
  | "totalUsageHours"
  | "cncMachineId"
  | "cncMachineName"
  | "operationType"
  | "trayNo";

export type FieldDef = {
  key: FieldKey;
  label: string;
  maxLength: number;
  /** ImGuiInputTextFlags_CharsDecimal was set on these inputs. */
  decimal?: boolean;
};

export const HOLDER_FIELDS: FieldDef[] = [
  { key: "id", label: "Holder ID:", maxLength: 127 },
  { key: "name", label: "Holder Name:", maxLength: 255 },
  { key: "calibrationStatus", label: "Calibration / Inspection Status:", maxLength: 127 },
  {
    key: "accumulatedLife",
    label: "Current Accumulated Life (e.g. 120.5 mins / 450 parts cut):",
    maxLength: 127,
  },
  {
    key: "serialBatchNumber",
    label: "Serial Number / Batch Number (Traceability code):",
    maxLength: 127,
  },
];

export const TOOL_FIELDS: FieldDef[] = [
  { key: "id", label: "Tool ID:", maxLength: 127 },
  { key: "name", label: "Tool Name:", maxLength: 255 },
  { key: "toolLife", label: "Tool Life (meters):", maxLength: 127, decimal: true },
  { key: "completedLife", label: "Completed Life (meters):", maxLength: 127, decimal: true },
  { key: "remainingVisual", label: "Remaining Life - Visual Inspection (meters):", maxLength: 127, decimal: true },
  { key: "remainingML", label: "Remaining Life - ML Model (meters):", maxLength: 127, decimal: true },
  { key: "totalUsageHours", label: "Total Hours of Usage:", maxLength: 127, decimal: true },
  { key: "cncMachineId", label: "CNC Machine ID:", maxLength: 127 },
  { key: "cncMachineName", label: "CNC Machine Name:", maxLength: 127 },
  { key: "operationType", label: "Type of Operation:", maxLength: 255 },
  { key: "trayNo", label: "Tray No:", maxLength: 127 },
];

/** Reset*InputBuffers() memsets every buffer to "" — mirror that exactly. */
export function emptyFields(type: QrType): Record<FieldKey, string> {
  const base = {} as Record<FieldKey, string>;
  for (const def of type === "Holder" ? HOLDER_FIELDS : TOOL_FIELDS) base[def.key] = "";
  return base;
}

/** The three quick-fill buttons next to the calibration status input. */
export const CALIBRATION_PRESETS: { label: string; value: string }[] = [
  { label: "Passed", value: "Passed" },
  { label: "Calibrated", value: "Calibrated" },
  { label: "Pending", value: "Pending Inspection" },
];

/** Exactly the four options from DrawSizeSelection(), in order. */
export const SIZE_OPTIONS = [
  { sizeChoice: 0, sizeMM: 9, depthMM: 0.8, label: "1.  9 x 9 mm  |  Depth 0.8 mm" },
  { sizeChoice: 1, sizeMM: 7, depthMM: 1.0, label: "2.  7 x 7 mm  |  Depth 1.0 mm" },
  { sizeChoice: 2, sizeMM: 5, depthMM: 1.2, label: "3.  5 x 5 mm  |  Depth 1.2 mm" },
  { sizeChoice: 3, sizeMM: 3, depthMM: 1.5, label: "4.  3 x 3 mm  |  Depth 1.5 mm" },
] as const;
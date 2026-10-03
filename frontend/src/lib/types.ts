/** Types mirroring the FastAPI schemas and the C++ QRObject. */

export type QrType = "Holder" | "Tool";
export type ExportKind = "stl" | "png";

/** Error codes produced by the C++ engine's validate command. */
export type EngineErrorCode =
  | "ok"
  | "empty_field"
  | "duplicate_id"
  | "validation_error"
  | "bad_type"
  | "unknown_command"
  | string;

export interface EngineError {
  code: EngineErrorCode;
  message: string;
  duplicateId?: string | null;
  path?: string;
  relativePath?: string;
  bytes?: number;
}

export interface SizeOption {
  sizeChoice: number;
  sizeMM: number;
  depthMM: number;
}

export interface OperationGroup {
  group: string;
  items: string[];
}

export interface QRObject {
  id: string;
  batchId: string;
  position: number;
  type: QrType;
  qrId: string;
  name: string;

  // Holder
  calibrationStatus: string;
  accumulatedLife: string;
  serialBatchNumber: string;

  // Tool
  toolLifeMeters: number;
  completedLifeMeters: number;
  remainingLifeVisualMeters: number;
  remainingLifeMLMeters: number;
  totalUsageHours: number;
  cncMachineId: string;
  cncMachineName: string;
  operationType: string;
  trayNo: string;

  sizeMM: number;
  depthMM: number;

  payload: string | null;
  matrixSize: number | null;
  triangleCount: number | null;

  stlGenerated: boolean;
  pngGenerated: boolean;
  stlBytes: number | null;
  pngBytes: number | null;
  createdAt: string | null;
}

export interface Batch {
  id: string;
  type: QrType;
  targetQuantity: number;
  sizeMM: number | null;
  depthMM: number | null;
  state: "entering" | "generated" | string;
  objectCount: number;
  createdAt: string | null;
  objects?: QRObject[];
}

export interface FileStatus {
  relativePath: string;
  path: string;
  exists: boolean;
  bytes: number | null;
}

export interface ObjectStatus {
  id: string;
  qrId: string;
  type: QrType;
  stl: FileStatus;
  png: FileStatus;
}

export interface ExportResult {
  ok: boolean;
  kind: ExportKind;
  path: string;
  relativePath: string;
  bytes: number;
}

export interface HealthInfo {
  status: string;
  database: { ok: boolean; error: string | null; url: string };
  engine: { ok: boolean; binary: string };
  outputRoot: string;
}

export interface EnginePing {
  available: boolean;
  engine: string;
  version: string;
  qrImplementation: string;
  pngDefaults: string;
  meshDefaults: string;
  outputRoot: string;
  binary: string;
}

/**
 * Raw string fields exactly as typed, matching the desktop app's m_Input*
 * char buffers. Numbers stay as strings so the C++ engine performs the same
 * parsing (and produces the same error messages) it always did.
 */
export type RawFields = Record<string, string>;

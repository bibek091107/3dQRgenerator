/**
 * Thin client for the FastAPI backend.
 *
 * Validation, payload building, QR generation, 3D meshing and STL/PNG writing
 * all happen in the C++ engine behind these endpoints — the browser never
 * re-implements any of it.
 */

import type {
  Batch,
  EngineError,
  EnginePing,
  ExportKind,
  ExportResult,
  HealthInfo,
  ObjectStatus,
  OperationGroup,
  QRObject,
  QrType,
  RawFields,
  SizeOption,
} from "./types";

const BASE =
  process.env.NEXT_PUBLIC_API_BASE?.replace(/\/$/, "") ?? "https://backend-indol-two-cumy47ky2b.vercel.app";

export class ApiError extends Error {
  readonly code: string;
  readonly status: number;
  readonly detail: EngineError;

  constructor(status: number, detail: EngineError) {
    super(detail.message || `HTTP ${status}`);
    this.name = "ApiError";
    this.status = status;
    this.code = detail.code ?? "unknown";
    this.detail = detail;
  }
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  let res: Response;
  try {
    res = await fetch(`${BASE}${path}`, {
      ...init,
      headers: { "Content-Type": "application/json", ...(init?.headers ?? {}) },
      cache: "no-store",
    });
  } catch {
    throw new ApiError(0, {
      code: "backend_unreachable",
      message: `Cannot reach the backend at ${BASE}. Is the FastAPI server running?`,
    });
  }

  if (res.status === 204) return undefined as T;

  const text = await res.text();
  let body: unknown = null;
  if (text) {
    try {
      body = JSON.parse(text);
    } catch {
      throw new ApiError(res.status, {
        code: "bad_response",
        message: text.slice(0, 400),
      });
    }
  }

  if (!res.ok) {
    const raw = (body as { detail?: unknown })?.detail;
    if (raw && typeof raw === "object" && "code" in (raw as object)) {
      throw new ApiError(res.status, raw as EngineError);
    }
    if (Array.isArray(raw)) {
      // FastAPI request-validation errors
      const first = raw[0] as { loc?: string[]; msg?: string } | undefined;
      throw new ApiError(res.status, {
        code: "validation_error",
        message: `${first?.loc?.slice(1).join(".") ?? "request"}: ${first?.msg ?? "invalid"}`,
      });
    }
    throw new ApiError(res.status, { code: "http_error", message: String(raw ?? res.statusText) });
  }

  return body as T;
}

function post<T>(path: string, payload: unknown): Promise<T> {
  return request<T>(path, { method: "POST", body: JSON.stringify(payload) });
}

export const api = {
  base: BASE,

  health: () => request<HealthInfo>("/api/health"),
  enginePing: () => request<EnginePing>("/api/engine/ping"),
  sizes: () => request<SizeOption[]>("/api/engine/sizes"),
  operations: () => request<OperationGroup[]>("/api/engine/operations"),

  createBatch: (type: QrType, quantity: number) =>
    post<Batch>("/api/batches", { type, quantity }),

  getBatch: (batchId: string) => request<Batch>(`/api/batches/${batchId}`),
  deleteBatch: (batchId: string) =>
    request<void>(`/api/batches/${batchId}`, { method: "DELETE" }),

  /**
   * Save one object. The C++ engine trims the fields, applies the "N/A"
   * defaults, runs the numeric validation and the duplicate-ID check, and
   * returns the desktop app's exact error codes/messages when it refuses.
   */
  createObject: (batchId: string, type: QrType, fields: RawFields) =>
    post<QRObject>(`/api/batches/${batchId}/objects`, { type, fields }),

  generate: (batchId: string, sizeChoice: number) =>
    post<Batch>(`/api/batches/${batchId}/generate`, { sizeChoice }),

  objectStatus: (objectId: string) => request<ObjectStatus>(`/api/objects/${objectId}/status`),

  exportObject: (objectId: string, kind: ExportKind, overwrite: boolean) =>
    post<ExportResult>(`/api/objects/${objectId}/export`, { kind, overwrite }),

  meshUrl: (objectId: string) => `${BASE}/api/objects/${objectId}/mesh`,
  downloadUrl: (objectId: string, kind: ExportKind) =>
    `${BASE}/api/objects/${objectId}/download?kind=${kind}`,

  /**
   * Save an exported file onto the device running the browser.
   *
   * The engine writes the canonical copy into the shared server-side output
   * folder; this pulls that same byte-exact file down so the operator ends up
   * with it in their own Downloads folder. Fetching the blob (rather than
   * following a plain link) keeps error handling in one place and avoids
   * navigating away from the app.
   */
  downloadToDevice: async (objectId: string, kind: ExportKind): Promise<string> => {
    const res = await fetch(`${BASE}/api/objects/${objectId}/download?kind=${kind}`);
    if (!res.ok) {
      const raw = await res.json().catch(() => null);
      throw new ApiError(res.status, (raw?.detail ?? { code: "http_error", message: res.statusText }));
    }
    const blob = await res.blob();
    const name =
      /filename="?([^"]+)"?/i.exec(res.headers.get("content-disposition") ?? "")?.[1] ??
      `${objectId}.${kind}`;

    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = name;
    a.rel = "noopener";
    document.body.appendChild(a);
    a.click();
    a.remove();
    // Give the browser a moment to start the download before revoking.
    setTimeout(() => URL.revokeObjectURL(url), 30_000);
    return name;
  },
};
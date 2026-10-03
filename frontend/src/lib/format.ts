/** Formatting helpers that mirror the desktop app's printf output. */

/** Mirrors snprintf("%.2f") used throughout App.cpp. */
export function fixed2(value: number): string {
  if (!Number.isFinite(value)) return "0.00";
  return value.toFixed(2);
}

/** Mirrors snprintf("%.1f") used for the size/depth display. */
export function fixed1(value: number): string {
  if (!Number.isFinite(value)) return "0.0";
  return value.toFixed(1);
}

/** Mirrors the QString("%d") style integer display. */
export function intLabel(value: number): string {
  return String(Math.trunc(value));
}

/**
 * Character filter matching ImGuiInputTextFlags_CharsDecimal.
 * ImGui's filter is: Integer 0123456789+-.
 */
export const CHARS_DECIMAL = "0123456789+-.";

export function sanitizeDecimal(input: string): string {
  return input
    .split("")
    .filter((ch) => CHARS_DECIMAL.includes(ch))
    .join("");
}

/** Quantity screen used ImGuiInputTextFlags_CharsDecimal as well. */
export function sanitizeQuantity(input: string): string {
  return sanitizeDecimal(input);
}

/**
 * Faithful port of std::stod for the live "Expected Remaining Life" helper.
 *
 * Returns null in every situation where the C++ call would throw
 * (invalid_argument / out_of_range), which is exactly when the desktop app
 * suppressed the helper line.
 */
export function stdStod(input: string): number | null {
  if (input.length === 0) return null;

  // std::stod skips leading whitespace and accepts INF / INFINITY / NAN.
  const trimmed = input.replace(/^\s+/, "");
  const special = /^[+-]?(INF(INITY)?|NAN)/i.exec(trimmed);
  if (special) {
    const sign = special[0].startsWith("-") ? -1 : 1;
    return /NAN/i.test(special[0]) ? Number.NaN : sign * Number.POSITIVE_INFINITY;
  }

  // std::stod consumes the longest valid prefix: "12abc" parses as 12.
  const m = /^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?/.exec(trimmed);
  if (!m) return null;

  const value = Number(m[0]);
  // std::stod throws out_of_range for values that do not fit in a double.
  if (!Number.isFinite(value)) return null;
  return value;
}

export function formatBytes(bytes: number | null | undefined): string {
  if (bytes === null || bytes === undefined) return "";
  if (bytes < 1024) return `${bytes} bytes`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
}

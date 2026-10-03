"use client";

/** Small presentational primitives styled after the original ImGui panel. */

import type { ReactNode } from "react";

export function Panel({
  children,
  className,
}: {
  children: ReactNode;
  className?: string;
}) {
  return (
    <section
      className={`rounded-lg border border-[#2e2e38] bg-[#1a1a1f] shadow-2xl shadow-black/40 ${className ?? ""}`}
    >
      {children}
    </section>
  );
}

export function PanelHeader({ title }: { title?: string }) {
  return (
    <header className="border-b border-[#2e2e38] bg-[#202027] px-4 py-2 text-xs font-semibold tracking-wide text-[#cfd2dc] uppercase">
      {title}
    </header>
  );
}

export function PanelBody({ children }: { children: ReactNode }) {
  return <div className="space-y-3 p-4">{children}</div>;
}

export function Banner({ children }: { children: ReactNode }) {
  return (
    <pre className="text-center text-sm font-bold leading-6 tracking-widest text-[#e6e8ef]">
      {children}
    </pre>
  );
}

export function Divider() {
  return <hr className="border-0 border-t border-[#33333d]" />;
}

type ButtonProps = {
  children: ReactNode;
  onClick?: () => void;
  variant?: "primary" | "default" | "danger";
  size?: "sm" | "md" | "lg";
  disabled?: boolean;
  type?: "button" | "submit";
  full?: boolean;
  title?: string;
};

export function Button({
  children,
  onClick,
  variant = "default",
  size = "md",
  disabled = false,
  type = "button",
  full = false,
  title,
}: ButtonProps) {
  const variants: Record<string, string> = {
    default:
      "bg-[#2a2a33] border-[#3d3d49] hover:bg-[#33333e] text-[#dfe2ea]",
    primary:
      "bg-[#2f6fb0] border-[#3d84c9] hover:bg-[#3a80c6] text-white",
    danger: "bg-[#7a3038] border-[#9a3d47] hover:bg-[#8d3841] text-white",
  };
  const sizes: Record<string, string> = {
    sm: "px-2.5 py-1 text-xs",
    md: "px-3.5 py-1.5 text-sm",
    lg: "px-4 py-2 text-sm",
  };
  return (
    <button
      type={type}
      title={title}
      onClick={onClick}
      disabled={disabled}
      className={`rounded border font-medium transition-colors disabled:cursor-not-allowed disabled:opacity-40 ${variants[variant]} ${sizes[size]} ${full ? "w-full" : ""}`}
    >
      {children}
    </button>
  );
}

export function Field({
  label,
  children,
  hint,
}: {
  label: string;
  children: ReactNode;
  hint?: ReactNode;
}) {
  return (
    <div className="space-y-1">
      <label className="block text-sm text-[#b9bdc9]">{label}</label>
      {children}
      {hint ? <div className="text-xs">{hint}</div> : null}
    </div>
  );
}

export function TextInput({
  value,
  onChange,
  placeholder,
  filter,
  maxLength,
  id,
  width,
}: {
  value: string;
  onChange: (next: string) => void;
  placeholder?: string;
  /** Mirrors an ImGuiInputTextFlags_* character filter. */
  filter?: (raw: string) => string;
  maxLength?: number;
  id?: string;
  width?: number;
}) {
  return (
    <input
      id={id}
      type="text"
      value={value}
      maxLength={maxLength}
      placeholder={placeholder}
      style={width ? { width } : undefined}
      onChange={(e) => {
        const raw = e.target.value;
        const next = filter ? filter(raw) : raw;
        // Respect the original char-buffer limits.
        onChange(maxLength ? next.slice(0, maxLength) : next);
      }}
      className="w-full rounded border border-[#3d3d49] bg-[#141419] px-2.5 py-1.5 text-sm text-[#e6e8ef] outline-none focus:border-[#4b8fd0] focus:ring-1 focus:ring-[#4b8fd0]/40"
    />
  );
}

export function SmallButton({
  children,
  onClick,
  title,
}: {
  children: ReactNode;
  onClick?: () => void;
  title?: string;
}) {
  return (
    <button
      type="button"
      title={title}
      onClick={onClick}
      className="rounded border border-[#3d3d49] bg-[#25252d] px-2 py-0.5 text-xs text-[#dfe2ea] hover:bg-[#2e2e38]"
    >
      {children}
    </button>
  );
}

export function Row({ children, gap = "gap-2" }: { children: ReactNode; gap?: string }) {
  return <div className={`flex flex-wrap items-center ${gap}`}>{children}</div>;
}

export function InfoLine({
  label,
  value,
  mono,
  color,
}: {
  label: string;
  value: ReactNode;
  mono?: boolean;
  color?: string;
}) {
  return (
    <div className="flex gap-2 text-sm">
      <span className="w-40 shrink-0 text-right text-[#9aa0ae]">{label}</span>
      <span
        className={`min-w-0 flex-1 break-words ${mono ? "font-mono" : ""}`}
        style={color ? { color } : undefined}
      >
        {value}
      </span>
    </div>
  );
}

export function SectionLabel({ children }: { children: ReactNode }) {
  return <div className="pt-1 text-xs tracking-widest text-[#8b909d] uppercase">{children}</div>;
}

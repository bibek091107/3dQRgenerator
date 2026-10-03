"use client";

/** Tool details entry — App::DrawToolDetailsMenu(). */

import { useEffect, useRef, useState } from "react";

import { api } from "@/lib/api";
import { fixed2, sanitizeDecimal, stdStod } from "@/lib/format";
import type { OperationGroup } from "@/lib/types";
import {
  Banner,
  Button,
  Divider,
  Field,
  Panel,
  PanelBody,
  Row,
  TextInput,
} from "./ui";

/**
 * Live "Expected Remaining Life" helper, ported from the desktop block that
 * reads m_InputToolLife / m_InputCompletedLife with std::stod and hides itself
 * when either value fails to parse or is negative.
 */
function RemainingLifeHint({ toolLife, completedLife }: { toolLife: string; completedLife: string }) {
  const tl = stdStod(toolLife);
  const cl = stdStod(completedLife);
  if (tl === null || cl === null || tl < 0 || cl < 0) return null;

  const expected = tl - cl;
  if (expected < 0) {
    return (
      <div className="pl-2 text-sm text-[#ff6666]">Warning: Completed Life exceeds Tool Life!</div>
    );
  }
  return (
    <div className="pl-2 text-sm text-[#66d9ff]">
      {`  Expected Remaining Life: ${fixed2(expected)} m (Tool Life - Completed Life)`}
    </div>
  );
}

function OperationDropdown({ onPick }: { onPick: (value: string) => void }) {
  const [open, setOpen] = useState(false);
  const [groups, setGroups] = useState<OperationGroup[]>([]);
  const wrapRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!open || groups.length) return;
    api
      .operations()
      .then(setGroups)
      .catch(() => setGroups([]));
  }, [open, groups.length]);

  useEffect(() => {
    if (!open) return;
    const onDocClick = (e: MouseEvent) => {
      if (wrapRef.current && !wrapRef.current.contains(e.target as Node)) setOpen(false);
    };
    document.addEventListener("mousedown", onDocClick);
    return () => document.removeEventListener("mousedown", onDocClick);
  }, [open]);

  return (
    <div className="relative" ref={wrapRef}>
      <button
        type="button"
        aria-label="Open operation list"
        onClick={() => setOpen((v) => !v)}
        className="h-[30px] w-7 shrink-0 rounded border border-[#3d3d49] bg-[#25252d] text-xs leading-none text-[#dfe2ea] hover:bg-[#2e2e38]"
      >
        ▼
      </button>
      {open ? (
        <div className="absolute right-0 top-full z-30 mt-1 max-h-72 w-72 overflow-auto rounded border border-[#3d3d49] bg-[#23232b] py-1 shadow-xl shadow-black/50">
          {groups.length === 0 ? (
            <div className="px-3 py-2 text-xs text-[#8b909d]">Loading…</div>
          ) : (
            groups.map((group, gi) => (
              <div key={group.group}>
                {gi > 0 ? <div className="my-1 border-t border-[#3a3a46]" /> : null}
                <div className="px-3 py-1 text-xs font-semibold text-[#66ccff]">{group.group}</div>
                {group.items.map((item) => (
                  <button
                    key={item}
                    type="button"
                    onClick={() => {
                      onPick(item);
                      setOpen(false);
                    }}
                    className="block w-full px-5 py-1 text-left text-sm text-[#e2e4ea] hover:bg-[#33333e]"
                  >
                    {item}
                  </button>
                ))}
              </div>
            ))
          )}
        </div>
      ) : null}
    </div>
  );
}

export function ToolDetails({
  index,
  total,
  values,
  onChange,
  onSave,
  onBack,
  busy,
}: {
  index: number;
  total: number;
  values: Record<string, string>;
  onChange: (key: string, next: string) => void;
  onSave: () => void;
  onBack: () => void;
  busy: boolean;
}) {
  const v = (k: string) => values[k] ?? "";
  const set = (k: string, next: string) => onChange(k, next);

  return (
    <Panel className="w-[600px]">
      <PanelBody>
        <Banner>{"====================================\n         TOOL INFORMATION           \n===================================="}</Banner>
        <div className="text-sm text-[#dfe2ea]">{`Tool ${index + 1} of ${total}`}</div>
        <Divider />

        <Field label="Tool ID:">
          <TextInput value={v("id")} maxLength={127} onChange={(x) => set("id", x)} />
        </Field>

        <Field label="Tool Name:">
          <TextInput value={v("name")} maxLength={255} onChange={(x) => set("name", x)} />
        </Field>

        <Field label="Tool Life (meters):">
          <TextInput
            value={v("toolLife")}
            maxLength={127}
            filter={sanitizeDecimal}
            onChange={(x) => set("toolLife", x)}
          />
        </Field>

        <Field label="Completed Life (meters):">
          <TextInput
            value={v("completedLife")}
            maxLength={127}
            filter={sanitizeDecimal}
            onChange={(x) => set("completedLife", x)}
          />
        </Field>

        <RemainingLifeHint toolLife={v("toolLife")} completedLife={v("completedLife")} />

        <Field label="Remaining Life - Visual Inspection (meters):">
          <TextInput
            value={v("remainingVisual")}
            maxLength={127}
            filter={sanitizeDecimal}
            onChange={(x) => set("remainingVisual", x)}
          />
        </Field>

        <Field label="Remaining Life - ML Model (meters):">
          <TextInput
            value={v("remainingML")}
            maxLength={127}
            filter={sanitizeDecimal}
            onChange={(x) => set("remainingML", x)}
          />
        </Field>

        <Field label="Total Hours of Usage:">
          <TextInput
            value={v("totalUsageHours")}
            maxLength={127}
            filter={sanitizeDecimal}
            onChange={(x) => set("totalUsageHours", x)}
          />
        </Field>

        <Field label="CNC Machine ID:">
          <TextInput value={v("cncMachineId")} maxLength={127} onChange={(x) => set("cncMachineId", x)} />
        </Field>

        <Field label="CNC Machine Name:">
          <TextInput value={v("cncMachineName")} maxLength={127} onChange={(x) => set("cncMachineName", x)} />
        </Field>

        <Field label="Type of Operation:">
          <Row>
            <TextInput
              value={v("operationType")}
              maxLength={255}
              onChange={(x) => set("operationType", x)}
            />
            <OperationDropdown onPick={(x) => set("operationType", x)} />
          </Row>
        </Field>

        <Field label="Tray No:">
          <TextInput value={v("trayNo")} maxLength={127} onChange={(x) => set("trayNo", x)} />
        </Field>

        <Divider />

        <Row gap="gap-3">
          <Button variant="primary" onClick={onSave} disabled={busy}>
            {index + 1 >= total ? "Save Tool" : "Next Tool"}
          </Button>
          <Button onClick={onBack} disabled={busy}>
            Back
          </Button>
        </Row>
      </PanelBody>
    </Panel>
  );
}
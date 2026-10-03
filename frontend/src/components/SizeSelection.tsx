"use client";

/** Size selection — App::DrawSizeSelection(). */

import { SIZE_OPTIONS } from "@/lib/fields";
import { Button, Divider, Panel, PanelBody } from "./ui";

export function SizeSelection({
  type,
  count,
  sizeChoice,
  onSizeChoice,
  onGenerate,
  busy,
}: {
  type: "Holder" | "Tool";
  count: number;
  sizeChoice: number;
  onSizeChoice: (choice: number) => void;
  onGenerate: () => void;
  busy: boolean;
}) {
  return (
    <Panel className="w-[460px]">
      <PanelBody>
        <div className="text-sm text-[#dfe2ea]">{`Type: ${type}  |  Count: ${count}`}</div>
        <Divider />
        <div className="text-sm text-[#dfe2ea]">Choose QR Size (applies to all items):</div>

        <div className="space-y-1 pt-1">
          {SIZE_OPTIONS.map((opt) => (
            <label
              key={opt.sizeChoice}
              className="flex cursor-pointer items-center gap-2 text-sm text-[#dfe2ea]"
            >
              <input
                type="radio"
                name="sizeChoice"
                checked={sizeChoice === opt.sizeChoice}
                onChange={() => onSizeChoice(opt.sizeChoice)}
                className="accent-[#4b8fd0]"
              />
              {opt.label}
            </label>
          ))}
        </div>

        {sizeChoice === 3 ? (
          <div className="pt-1 text-sm font-semibold text-[#ffff00]">
            WARNING: 3mm QR codes may be hard to scan.
          </div>
        ) : null}

        <div className="pt-1">
          <Button variant="primary" onClick={onGenerate} disabled={busy}>
            Generate QRs and Preview
          </Button>
        </div>
      </PanelBody>
    </Panel>
  );
}
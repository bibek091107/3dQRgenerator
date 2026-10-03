"use client";

/** Quantity entry — App::DrawQuantityMenu(). */

import { Button, Divider, Field, Panel, PanelBody, TextInput } from "./ui";
import { sanitizeQuantity } from "@/lib/format";

export function QuantityScreen({
  type,
  value,
  onChange,
  onNext,
  onBack,
}: {
  type: "Holder" | "Tool";
  value: string;
  onChange: (next: string) => void;
  onNext: () => void;
  onBack: () => void;
}) {
  return (
    <Panel className="w-[460px]">
      <PanelBody>
        <div className="text-sm text-[#dfe2ea]">{`Type: ${type}`}</div>
        <Divider />
        <div className="text-sm text-[#dfe2ea]">How many QR codes do you want to generate?</div>

        <Field label="Quantity">
          <TextInput
            value={value}
            onChange={onChange}
            // ImGuiInputTextFlags_CharsDecimal
            filter={sanitizeQuantity}
            maxLength={15}
          />
        </Field>

        <div className="flex gap-3 pt-1">
          <Button variant="primary" onClick={onNext}>
            Next
          </Button>
          <Button onClick={onBack}>Back</Button>
        </div>
      </PanelBody>
    </Panel>
  );
}
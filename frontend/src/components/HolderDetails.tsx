"use client";

/** Holder details entry — App::DrawHolderDetailsMenu(). */

import { CALIBRATION_PRESETS } from "@/lib/fields";
import {
  Banner,
  Button,
  Divider,
  Field,
  Panel,
  PanelBody,
  Row,
  SmallButton,
  TextInput,
} from "./ui";

export function HolderDetails({
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
  return (
    <Panel className="w-[560px]">
      <PanelBody>
        <Banner>{"====================================\n        HOLDER INFORMATION          \n===================================="}</Banner>
        <div className="text-sm text-[#dfe2ea]">{`Holder ${index + 1} of ${total}`}</div>
        <Divider />

        <Field label="Holder ID:">
          <TextInput value={values.id ?? ""} maxLength={127} onChange={(v) => onChange("id", v)} />
        </Field>

        <Field label="Holder Name:">
          <TextInput value={values.name ?? ""} maxLength={255} onChange={(v) => onChange("name", v)} />
        </Field>

        <Field label="Calibration / Inspection Status:">
          <Row>
            <TextInput
              value={values.calibrationStatus ?? ""}
              maxLength={127}
              onChange={(v) => onChange("calibrationStatus", v)}
            />
            {CALIBRATION_PRESETS.map((preset) => (
              <SmallButton
                key={preset.label}
                onClick={() => onChange("calibrationStatus", preset.value)}
              >
                {preset.label}
              </SmallButton>
            ))}
          </Row>
        </Field>

        <Field label="Current Accumulated Life (e.g. 120.5 mins / 450 parts cut):">
          <TextInput
            value={values.accumulatedLife ?? ""}
            maxLength={127}
            onChange={(v) => onChange("accumulatedLife", v)}
          />
        </Field>

        <Field label="Serial Number / Batch Number (Traceability code):">
          <TextInput
            value={values.serialBatchNumber ?? ""}
            maxLength={127}
            onChange={(v) => onChange("serialBatchNumber", v)}
          />
        </Field>

        <Divider />

        <Row gap="gap-3">
          <Button variant="primary" onClick={onSave} disabled={busy}>
            {index + 1 >= total ? "Save Holder" : "Next Holder"}
          </Button>
          <Button onClick={onBack} disabled={busy}>
            Back
          </Button>
        </Row>
      </PanelBody>
    </Panel>
  );
}
"use client";

/** Preview panel — App::DrawPreviewMenu(). */

import { fixed1, fixed2 } from "@/lib/format";
import type { ObjectStatus, QRObject } from "@/lib/types";
import {
  Banner,
  Button,
  Divider,
  InfoLine,
  Panel,
  PanelBody,
  Row,
} from "./ui";

function FileStatusLine({
  label,
  path,
  exists,
}: {
  label: string;
  path: string;
  exists: boolean;
}) {
  return (
    <>
      <div className="text-sm break-all text-[#dfe2ea]">
        {`${label}: ${path}`}
      </div>
      <div
        className="pl-2 text-sm"
        style={{ color: exists ? "#33ff66" : "#b3b3b3" }}
      >
        {exists ? "  Status: Generated ✓" : "  Status: Not generated"}
      </div>
    </>
  );
}

export function PreviewScreen({
  objects,
  index,
  status,
  onPrev,
  onNext,
  onExportCurrent,
  onExportAll,
  onBack,
  busy,
}: {
  objects: QRObject[];
  index: number;
  status: ObjectStatus | null;
  onPrev: () => void;
  onNext: () => void;
  onExportCurrent: (kind: "stl" | "png") => void;
  onExportAll: (kind: "stl" | "png") => void;
  onBack: () => void;
  busy: boolean;
}) {
  const cur = objects[index];
  if (!cur) return null;

  const stl = status?.stl;
  const png = status?.png;

  return (
    <Panel className="w-[560px]">
      <PanelBody>
        <Banner>{"--------------------------------------------------\n               3D QR CODE PREVIEW\n--------------------------------------------------"}</Banner>
        <div className="text-sm text-[#dfe2ea]">{`  QR ${index + 1} / ${objects.length}`}</div>
        <Divider />

        <InfoLine label="Type:" value={cur.type} />
        <InfoLine label="ID:" value={cur.qrId} />
        <InfoLine label="Name:" value={cur.name} />

        {cur.type === "Holder" ? (
          <>
            <InfoLine label="Calib. Status:" value={cur.calibrationStatus} />
            <InfoLine label="Accum. Life:" value={cur.accumulatedLife} />
            <InfoLine label="Serial/Batch:" value={cur.serialBatchNumber} />
          </>
        ) : (
          <>
            <InfoLine label="Tool Life:" value={`${fixed2(cur.toolLifeMeters)} m`} />
            <InfoLine label="Completed Life:" value={`${fixed2(cur.completedLifeMeters)} m`} />
            <InfoLine label="Rem. (Visual):" value={`${fixed2(cur.remainingLifeVisualMeters)} m`} />
            <InfoLine label="Rem. (ML):" value={`${fixed2(cur.remainingLifeMLMeters)} m`} />
            <InfoLine label="Total Usage:" value={`${fixed2(cur.totalUsageHours)} hrs`} />
            <InfoLine label="CNC ID:" value={cur.cncMachineId} />
            <InfoLine label="CNC Name:" value={cur.cncMachineName} />
            <InfoLine label="Operation:" value={cur.operationType} />
            <InfoLine label="Tray No:" value={cur.trayNo} />
          </>
        )}

        <InfoLine label="Size:" value={`${fixed1(cur.sizeMM)} x ${fixed1(cur.sizeMM)} mm`} />
        <InfoLine label="Depth:" value={`${fixed1(cur.depthMM)} mm`} />

        <div className="space-y-1 pt-1">
          <FileStatusLine label="STL" path={stl?.path ?? `${cur.qrId}.stl`} exists={!!stl?.exists} />
          <FileStatusLine label="PNG" path={png?.path ?? `${cur.qrId}.png`} exists={!!png?.exists} />
        </div>

        <Divider />

        <Row gap="gap-3">
          <Button onClick={onPrev} disabled={index <= 0}>
            &lt; Previous
          </Button>
          <Button onClick={onNext} disabled={index >= objects.length - 1}>
            Next &gt;
          </Button>
        </Row>

        <Divider />

        <div className="grid grid-cols-2 gap-2">
          <Button onClick={() => onExportCurrent("stl")} disabled={busy}>
            Export Current STL
          </Button>
          <Button onClick={() => onExportCurrent("png")} disabled={busy}>
            Export Current PNG
          </Button>
          <Button onClick={() => onExportAll("stl")} disabled={busy}>
            Generate All STL
          </Button>
          <Button onClick={() => onExportAll("png")} disabled={busy}>
            Generate All PNG
          </Button>
        </div>

        <Row gap="gap-3">
          <Button onClick={onBack} disabled={busy}>
            Back to Main Menu
          </Button>
        </Row>
      </PanelBody>
    </Panel>
  );
}
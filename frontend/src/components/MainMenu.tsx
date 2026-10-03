"use client";

/** Main menu — App::DrawMainMenu(). */

import { Banner, Button, Divider, Panel, PanelBody, PanelHeader } from "./ui";

export function MainMenu({
  onSelect,
  onExit,
}: {
  onSelect: (type: "Holder" | "Tool") => void;
  onExit: () => void;
}) {
  return (
    <Panel className="w-[380px]">
      <PanelHeader title="3D QR Code Generator" />
      <PanelBody>
        <Banner>
          {"====================================\n      3D QR CODE GENERATOR         \n===================================="}
        </Banner>

        <p className="pt-1 text-center text-sm text-[#dfe2ea]">Choose Material Type:</p>

        <div className="flex flex-col items-center gap-2 pt-1">
          <Button size="lg" onClick={() => onSelect("Holder")}>
            HOLDER
          </Button>
          <Button size="lg" onClick={() => onSelect("Tool")}>
            TOOL
          </Button>
        </div>

        <Divider />

        <div className="flex justify-center">
          <Button size="lg" onClick={onExit} title="Close the application">
            EXIT
          </Button>
        </div>
      </PanelBody>
    </Panel>
  );
}
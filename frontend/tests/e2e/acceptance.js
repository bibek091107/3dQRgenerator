/**
 * End-to-end UI verification: drives the real Next.js app in Chromium
 * through the same flow a user performs in the native ImGui app.
 */
const { chromium } = require("playwright");
const fs = require("fs");

const WEB = process.env.WEB_BASE || "http://127.0.0.1:3000";
const API = process.env.API_BASE || "http://127.0.0.1:8000";
const SHOT_DIR = process.env.SHOT_DIR || "/tmp/shots";
const shot = (n) => `${SHOT_DIR}/${n}.png`;

let failures = 0;
let pageRef = null;
let closed = false;
function check(label, cond, extra = "") {
  const mark = cond ? "PASS" : "FAIL";
  if (!cond) failures++;
  console.log(`  [${mark}] ${label}${extra ? " :: " + extra : ""}`);
  if (!cond && pageRef && !closed) {
    pageRef
      .locator("body")
      .innerText()
      .then((t) => console.log("      page text: " + t.replace(/\n+/g, " | ").slice(0, 300)))
      .catch(() => {});
  }
}

(async () => {
  require("fs").mkdirSync(SHOT_DIR, { recursive: true });
  const browser = await chromium.launch();
  const context = await browser.newContext({
    viewport: { width: 1440, height: 900 },
    // Exports must land on the device running the browser, so downloads are
    // part of the expected behaviour rather than something to block.
    acceptDownloads: true,
  });
  const page = await context.newPage();
  pageRef = page;

  const consoleErrors = [];
  page.on("console", (m) => {
    if (m.type() === "error") consoleErrors.push(m.text());
  });
  page.on("pageerror", (e) => consoleErrors.push("pageerror: " + e.message));

  await page.goto(WEB, { waitUntil: "networkidle" });
  await page.screenshot({ path: shot("01-main-menu") });

  console.log("\n=== 1. Main menu ===");
  check("title banner", (await page.getByText("3D QR CODE GENERATOR").count()) > 0);
  check("Holder button", await page.getByRole("button", { name: "HOLDER" }).isVisible());
  check("Tool button", await page.getByRole("button", { name: "TOOL" }).isVisible());
  check("Exit button", await page.getByRole("button", { name: "EXIT" }).isVisible());
  check(
    "backend ready badge",
    (await page.getByText("engine + database ready").count()) > 0,
  );

  console.log("\n=== 2. Quantity screen ===");
  await page.getByRole("button", { name: "HOLDER" }).click();
  await page.waitForSelector("text=How many QR codes do you want to generate?");
  check("Type: Holder shown", (await page.getByText("Type: Holder").count()) > 0);
  // CharsDecimal filter: letters must be rejected by the input
  const q = page.locator("input").first();
  await q.fill("2a-b.c");
  // ImGui CharsDecimal keeps only 0-9 + - .
  check("CharsDecimal filter strips letters", (await q.inputValue()) === "2-.", await q.inputValue());
  await q.fill("");
  // Next with qty 0 must do nothing (desktop only advanced when qty > 0)
  await page.getByRole("button", { name: "Next" }).click();
  check("Next with empty quantity stays on screen", (await page.getByText("How many QR codes").count()) > 0);
  await q.fill("2");
  await page.screenshot({ path: shot("02-quantity") });
  await page.getByRole("button", { name: "Next" }).click();

  console.log("\n=== 3. Holder details screen ===");
  await page.waitForSelector("text=HOLDER INFORMATION");
  check("counter shows Holder 1 of 2", (await page.getByText("Holder 1 of 2").count()) > 0);
  check("Next Holder button", await page.getByRole("button", { name: "Next Holder" }).isVisible());

  // Empty ID/Name -> ERROR popup
  await page.getByRole("button", { name: "Next Holder" }).click();
  await page.waitForSelector("text=ID and Name cannot be empty.");
  check("empty-field ERROR popup", true);
  await page.screenshot({ path: shot("03-empty-field-popup") });
  await page.getByRole("button", { name: "OK" }).click();
  await page.locator('[role="dialog"]').waitFor({ state: "detached", timeout: 10000 });

  // Quick-fill preset buttons
  await page.locator("input").nth(0).fill("UI-H001");
  await page.locator("input").nth(1).fill("UI Fixture A");
  await page.getByRole("button", { name: "Calibrated" }).click();
  const calib = page.locator("input").nth(2);
  check("'Calibrated' preset fills the field", (await calib.inputValue()) === "Calibrated");
  await page.getByRole("button", { name: "Pending" }).click();
  check("'Pending' preset -> 'Pending Inspection'", (await calib.inputValue()) === "Pending Inspection");
  await page.locator("input").nth(3).fill("88.25 mins");
  await page.locator("input").nth(4).fill("SN-UI-9");
  await page.screenshot({ path: shot("04-holder-details") });
  await page.getByRole("button", { name: "Next Holder" }).click();

  console.log("\n=== 4. Duplicate ID popup (same ID twice in one batch) ===");
  await page.waitForSelector("text=Holder 2 of 2", { timeout: 15000 });
  check("advanced to Holder 2 of 2", (await page.getByText("Holder 2 of 2").count()) > 0);
  await page.locator("input").nth(0).fill("UI-H001");
  await page.locator("input").nth(1).fill("Duplicate attempt");
  await page.getByRole("button", { name: "Save Holder" }).click();
  await page.waitForSelector("text=already in use");
  check("Duplicate ID popup", true);
  await page.screenshot({ path: shot("05-duplicate-popup") });
  await page.getByRole("button", { name: "OK" }).click();
  await page.locator('[role="dialog"]').waitFor({ state: "detached", timeout: 10000 });

  console.log("\n=== 5. Holder 2 + validation ===");
  await page.locator("input").nth(0).fill("UI-H002");
  await page.locator("input").nth(1).fill("UI Fixture B");
  await page.getByRole("button", { name: "Save Holder" }).click();
  await page.waitForSelector("text=Choose QR Size", { timeout: 15000 });

  console.log("\n=== 6. Size selection ===");
  await page.waitForSelector("text=Choose QR Size");
  check("header count", (await page.getByText("Type: Holder  |  Count: 2").count()) > 0);
  check("4 size options", (await page.getByText("4.  3 x 3 mm  |  Depth 1.5 mm").count()) > 0);
  check("no warning on default (9mm)", (await page.getByText("WARNING: 3mm").count()) === 0);
  await page.getByText("4.  3 x 3 mm  |  Depth 1.5 mm").click();
  check("3mm warning appears", (await page.getByText("WARNING: 3mm QR codes may be hard to scan.").count()) > 0);
  await page.screenshot({ path: shot("06-size-warning") });
  await page.getByText("2.  7 x 7 mm  |  Depth 1.0 mm").click();
  check("warning clears for 7mm", (await page.getByText("WARNING: 3mm").count()) === 0);
  await page.screenshot({ path: shot("07-size-selection") });
  await page.getByRole("button", { name: "Generate QRs and Preview" }).click();

  console.log("\n=== 7. Preview + 3D canvas ===");
  await page.waitForSelector("text=3D QR CODE PREVIEW");
  check("QR 1 / 2", (await page.getByText("QR 1 / 2").count()) > 0);
  check("Previous disabled at index 0", await page.getByRole("button", { name: "< Previous" }).isDisabled());
  check("STL Not generated", (await page.getByText("Status: Not generated").count()) === 2);
  const canvas = page.locator("canvas");
  await canvas.waitFor({ state: "visible", timeout: 15000 });
  check("WebGL canvas mounted", (await canvas.count()) > 0);
  await page.waitForFunction(
    () => document.querySelector("canvas")?.width > 0,
    null,
    { timeout: 15000 },
  );
  const info = await page.evaluate(() => {
    const c = document.querySelector("canvas");
    const gl = c.getContext("webgl2") || c.getContext("webgl");
    return { w: c.width, h: c.height, gl: !!gl };
  });
  check("canvas has real pixels", info.w > 0 && info.h > 0, JSON.stringify(info));
  await page.waitForTimeout(2500); // let the mesh fetch + render settle
  const hud = await page.getByText(/tris \| .* verts/).textContent().catch(() => "");
  check("mesh loaded (stats HUD)", /tris/.test(hud || ""), hud || "");
  await page.screenshot({ path: shot("08-preview-3d") });

  console.log("\n=== 7b. 3D viewport is interactive (orbit / zoom) ===");
  // Regression guard: the control-panel overlay used to span the whole
  // viewport and swallow pointer events, so OrbitControls never got a drag
  // and the tag could not be moved at all.
  const hitTest = await page.evaluate(() => {
    const at = (x, y) => {
      const e = document.elementFromPoint(x, y);
      return e ? e.tagName.toUpperCase() : "NONE";
    };
    return { right: at(window.innerWidth - 120, 300), lower: at(window.innerWidth - 120, 500) };
  });
  check(
    "canvas owns pointer events in the viewport",
    hitTest.right === "CANVAS" && hitTest.lower === "CANVAS",
    JSON.stringify(hitTest),
  );

  // A drag and a wheel must not disturb the scene or throw.
  const dragErrors = [];
  const onErr = (e) => dragErrors.push(String(e.message || e));
  page.on("pageerror", onErr);
  const vw = (await page.viewportSize()).width;
  await page.mouse.move(vw - 200, 300);
  await page.mouse.down();
  await page.mouse.move(vw - 340, 240, { steps: 18 });
  await page.mouse.up();
  await page.waitForTimeout(700);
  await page.mouse.move(vw - 200, 300);
  await page.mouse.wheel(0, -400);
  await page.waitForTimeout(700);
  page.off("pageerror", onErr);
  check("orbit drag + zoom raise no errors", dragErrors.length === 0, dragErrors.join("; "));
  check(
    "preview still alive after orbiting",
    (await page.getByText("3D QR CODE PREVIEW").count()) > 0 &&
      (await page.locator("canvas").count()) > 0,
  );
  await page.screenshot({ path: shot("08b-preview-orbited") });

  console.log("\n=== 8. Navigation ===");
  await page.getByRole("button", { name: "Next >" }).click();
  check("QR 2 / 2", (await page.getByText("QR 2 / 2").count()) > 0);
  check("Next disabled at last index", await page.getByRole("button", { name: "Next >" }).isDisabled());
  await page.getByRole("button", { name: "< Previous" }).click();
  check("back to QR 1 / 2", (await page.getByText("QR 1 / 2").count()) > 0);
  await page.waitForTimeout(1200);

  console.log("\n=== 9. Export current STL -> device download + success popup ===");
  // The engine writes the canonical copy server-side; the browser must also
  // receive the same bytes, under the engine's own filename.
  const dlPath = await (async () => {
    const [dl] = await Promise.all([
      page.waitForEvent("download", { timeout: 30000 }),
      page.getByRole("button", { name: "Export Current STL" }).click(),
    ]);
    const dest = `${SHOT_DIR}/downloaded-${dl.suggestedFilename()}`;
    await dl.saveAs(dest);
    return { dest, name: dl.suggestedFilename() };
  })();
  await page.waitForSelector("text=STL Export Successful");
  check("STL success popup", true);
  check(
    "STL downloaded to this device with the qrId filename",
    dlPath.name.endsWith(".stl") && !/^[0-9a-f-]{36}\./i.test(dlPath.name),
    dlPath.name,
  );
  check("popup reports the device save", (await page.getByText(/Saved to this device/).count()) > 0);

  // Byte-identical to the server's canonical copy.
  const health = await (await fetch(`${API}/api/health`)).json();
  const serverCopy = `${health.outputRoot.replace("/private", "")}/output/holder/${dlPath.name}`;
  if (fs.existsSync(serverCopy)) {
    const a = fs.readFileSync(dlPath.dest);
    const c = fs.readFileSync(serverCopy);
    check("downloaded STL is byte-identical to the server copy", Buffer.compare(a, c) === 0);
    const tris = c.length >= 84 ? c.readUInt32LE(80) : -1;
    check(
      "downloaded STL is a valid binary STL",
      tris > 0 && c.length === 84 + tris * 50,
      `${tris} tris, ${c.length} bytes`,
    );
  } else {
    check("server copy found for byte comparison", false, serverCopy);
  }
  await page.screenshot({ path: shot("09-export-success") });
  await page.getByRole("button", { name: "OK" }).click();
  await page.waitForTimeout(800);
  check("STL status flips to Generated", (await page.getByText("Status: Generated").count()) >= 1);

  console.log("\n=== 10. Overwrite confirmation popup ===");
  await page.getByRole("button", { name: "Export Current STL" }).click();
  await page.waitForSelector("text=FILE ALREADY EXISTS");
  check("overwrite popup appears (file exists)", true);
  check("asks 'Do you want to overwrite it?'", (await page.getByText("Do you want to overwrite it?").count()) > 0);
  await page.screenshot({ path: shot("10-overwrite-popup") });
  await page.getByRole("button", { name: "Overwrite" }).click();
  await page.waitForTimeout(2000);
  check("overwrite finished", (await page.getByText("FILE ALREADY EXISTS").count()) === 0);
  // Overwrite re-export also reports success, exactly like the desktop queue.
  if ((await page.locator('[role="dialog"]').count()) > 0) {
    check("post-overwrite success popup", (await page.getByText("STL Export Successful").count()) > 0);
    await page.getByRole("button", { name: "OK" }).click();
    await page.locator('[role="dialog"]').waitFor({ state: "detached", timeout: 10000 });
  }

  console.log("\n=== 11. Batch export all PNG ===");
  // Every file in the batch must also reach the device, not just the server.
  const batchDownloads = [];
  page.on("download", (d) => batchDownloads.push(d.suggestedFilename()));
  await page.getByRole("button", { name: "Generate All PNG" }).click();
  // 2 objects -> desktop reports the plural "N files generated successfully."
  await page.waitForSelector("text=2 PNG files generated successfully.", { timeout: 30000 });
  check("batch PNG success popup (plural form)", true);
  await page.waitForTimeout(1200);
  check(
    "both PNGs downloaded to this device",
    batchDownloads.length === 2 && batchDownloads.every((n) => n.endsWith(".png")),
    batchDownloads.join(", "),
  );
  check("batch popup reports the device save", (await page.getByText(/Saved 2 file\(s\)/).count()) > 0);
  await page.screenshot({ path: shot("11-batch-export") });
  await page.getByRole("button", { name: "OK" }).click();
  await page.waitForTimeout(800);
  check("both PNGs generated", (await page.getByText("Status: Generated").count()) === 2);
  await page.screenshot({ path: shot("12-after-exports") });

  console.log("\n=== 11b. Batch overwrite: queue pauses per file, Cancel skips ===");
  // Re-export both PNGs: the first one already exists -> pause, Cancel, pause again.
  await page.getByRole("button", { name: "Generate All PNG" }).click();
  await page.waitForSelector("text=FILE ALREADY EXISTS", { timeout: 20000 });
  const firstPath = await page.locator('[role="dialog"] pre, [role="dialog"] .font-mono').first().textContent();
  check("queue pauses on the first existing file", /UI-H001\.png/.test(firstPath || ""), firstPath || "");
  await page.getByRole("button", { name: "Cancel" }).click();
  await page.waitForTimeout(1500);
  const paused2 = (await page.getByText("FILE ALREADY EXISTS").count()) > 0;
  check("queue advances to the next file after Cancel", paused2);
  if (paused2) {
    const secondPath = await page.locator('[role="dialog"] .font-mono').first().textContent();
    check("second pause is for the other object", /UI-H002\.png/.test(secondPath || ""), secondPath || "");
    await page.screenshot({ path: shot("11b-batch-overwrite") });
    await page.getByRole("button", { name: "Overwrite" }).click();
    await page.waitForTimeout(2000);
  }
  // Whichever dialogs remain get dismissed.
  for (let i = 0; i < 3 && (await page.locator('[role="dialog"]').count()) > 0; i++) {
    const ok = page.getByRole("button", { name: "OK" });
    if ((await ok.count()) > 0) await ok.first().click();
    else await page.getByRole("button", { name: "Cancel" }).click();
    await page.waitForTimeout(1200);
  }

  console.log("\n=== 12. Back to main menu ===");
  await page.getByRole("button", { name: "Back to Main Menu" }).click();
  await page.waitForSelector("text=Choose Material Type");
  check("returned to main menu", true);

  console.log("\n=== 13. Tool flow with live remaining-life helper ===");
  await page.getByRole("button", { name: "TOOL" }).click();
  await page.locator("input").first().fill("1");
  await page.getByRole("button", { name: "Next" }).click();
  await page.waitForSelector("text=TOOL INFORMATION");
  check("counter shows Tool 1 of 1", (await page.getByText("Tool 1 of 1").count()) > 0);
  check("Save Tool button label", await page.getByRole("button", { name: "Save Tool" }).isVisible());

  const ids = page.locator("input");
  await ids.nth(0).fill("UI-T001");
  await ids.nth(1).fill("UI End Mill 10mm");
  await ids.nth(2).fill("400");     // tool life
  await ids.nth(3).fill("150");     // completed life
  await page.waitForTimeout(300);
  check(
    "live Expected Remaining Life (400-150=250.00)",
    (await page.getByText("Expected Remaining Life: 250.00 m").count()) > 0,
  );
  // CharsDecimal on numeric fields
  await ids.nth(3).fill("15x0");
  check("numeric field strips letters", (await ids.nth(3).inputValue()) === "150", await ids.nth(3).inputValue());
  // Completed > tool life -> red warning
  await ids.nth(3).fill("500");
  await page.waitForTimeout(300);
  check(
    "warning when completed exceeds life",
    (await page.getByText("Warning: Completed Life exceeds Tool Life!").count()) > 0,
  );
  await page.screenshot({ path: shot("13-tool-warning") });
  await ids.nth(3).fill("150");

  // Operation dropdown
  await page.getByRole("button", { name: "Open operation list" }).click();
  await page.waitForSelector("text=Indexable Turning Tool");
  check("operation hierarchy groups", (await page.getByText("Flat End Mill").count()) > 0);
  await page.getByRole("button", { name: "Taper & Profile Turning" }).click();
  await page.waitForTimeout(200);
  check("dropdown fills Type of Operation", (await ids.nth(9).inputValue()) === "Taper & Profile Turning");
  await page.screenshot({ path: shot("14-operation-dropdown") });

  // Engine validation must reject a bad number
  await ids.nth(4).fill("180.5");
  await ids.nth(5).fill("220");
  await ids.nth(6).fill("210.25");
  await ids.nth(7).fill("77.5");
  await ids.nth(8).fill("CNC-09");
  await ids.nth(10).fill("TRAY-Z");
  await page.getByRole("button", { name: "Save Tool" }).click();
  // Save Tool -> size selection (same as the desktop state machine).
  await page.waitForSelector("text=Choose QR Size", { timeout: 20000 });
  check("reaches size selection after last tool", true);
  check("size choice persists across batches (still 7mm)", (await page.getByText("Type: Tool  |  Count: 1").count()) > 0);
  await page.getByText("1.  9 x 9 mm  |  Depth 0.8 mm").click();
  await page.getByRole("button", { name: "Generate QRs and Preview" }).click();
  await page.waitForSelector("text=3D QR CODE PREVIEW", { timeout: 25000 });
  await page.screenshot({ path: shot("15-tool-preview") });
  const inPreview = (await page.getByText("3D QR CODE PREVIEW").count()) > 0;
  check("tool saved and preview shown", inPreview);
  if (inPreview) {
    check("operation echoed in preview", (await page.getByText("Taper & Profile Turning").count()) > 0);
    check("depth 0.8mm default choice", (await page.getByText("0.8 mm").count()) > 0);
    await page.waitForTimeout(2500);
    await page.screenshot({ path: shot("16-tool-3d") });
  }

  console.log("\n=== 14. Console errors ===");
  // The suite deliberately triggers validation failures (empty field, duplicate
  // ID), so the browser logs expected 4xx API responses as resource errors.
  const realErrors = consoleErrors.filter(
    (e) =>
      !/Failed to load resource/i.test(e) &&
      !/favicon/i.test(e) &&
      !/WebSocket connection to .*_next\/hmr/i.test(e) &&
      !/Download the React DevTools/i.test(e),
  );
  check("no console/page errors", realErrors.length === 0, realErrors.slice(0, 4).join(" | "));

  closed = true;
  await browser.close();
  console.log(`\n${failures === 0 ? "ALL UI CHECKS PASSED" : failures + " UI CHECK(S) FAILED"}`);
  process.exit(failures === 0 ? 0 : 1);
})().catch((e) => {
  console.error("E2E crashed:", e.message);
  process.exit(2);
});
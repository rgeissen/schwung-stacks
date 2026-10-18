// shots.js -- Stacks' panel as it is, for build.py: the top bar, the staff,
// the chord strip, and each tab's pane with the names of its controls. The
// panel runs in its own demo mode (no bridge: a working progression, Am F C
// G7 Dm, playing), so this needs no Move and no Push; the DEMO badge is
// hidden for the pictures.
//
//   node shots.js <path to web_ui.html> <out dir>
// CHROME names the browser (default: Google Chrome on macOS).
const { chromium } = require("playwright-core");
const fs = require("fs");
const path = require("path");
const [PAGE, OUT] = process.argv.slice(2);
const CHROME = process.env.CHROME || "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const b = await chromium.launch({ executablePath: CHROME, headless: true });
  const p = await b.newPage({ viewport: { width: 1280, height: 1000 }, deviceScaleFactor: 1 });
  const errs = []; p.on("pageerror", e => errs.push(e.message));
  await p.goto("file://" + path.resolve(PAGE)); await p.waitForTimeout(1500);
  await p.addStyleTag({ content: "#demoBadge{display:none!important}" });
  await p.mouse.move(2, 2);
  const jpg = async sel => (await (await p.$(sel)).screenshot({ type: "jpeg", quality: 82 })).toString("base64");
  const out = { full: "", top: "", roll: "", strip: "", tabs: [], errors: errs };
  out.top = await jpg("header");
  out.roll = await jpg("section.roll");
  out.strip = await jpg("#strip");
  const n = (await p.$$(".tab")).length;
  for (let i = 0; i < n; i++) { // a click rebuilds the tabs: find them again each time
    const tab = (await p.$$(".tab"))[i];
    const name = (await tab.textContent()).trim();
    await tab.click(); await p.waitForTimeout(500); await p.mouse.move(2, 2);
    // Every control in the pane, in order: a labelled box, or an action button.
    const labels = await p.$$eval("#pane > .ctl", cs => cs.map(c =>
      c.classList.contains("act-btn") ? c.getAttribute("aria-label") : (c.querySelector("label .k") || {}).textContent || "").filter(Boolean));
    const scope = (await p.$eval("#scope", e => e.textContent)).trim();
    out.tabs.push({ name, scope, labels, shot: await jpg(".rail") });
  }
  await (await p.$$(".tab"))[0].click(); await p.waitForTimeout(400); await p.mouse.move(2, 2);
  out.full = (await p.screenshot({ type: "jpeg", quality: 80 })).toString("base64");
  fs.writeFileSync(OUT + "/data.json", JSON.stringify(out, null, 1));
  await b.close();
  if (errs.length) { console.error("script errors:", errs); process.exit(1); }
  console.log("tabs:", out.tabs.map(t => t.name + "(" + t.labels.length + ")").join(" "));
})().catch(e => { console.error(e); process.exit(1); });

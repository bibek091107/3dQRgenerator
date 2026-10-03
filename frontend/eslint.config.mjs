import { defineConfig, globalIgnores } from "eslint/config";
import nextVitals from "eslint-config-next/core-web-vitals";
import nextTs from "eslint-config-next/typescript";

const eslintConfig = defineConfig([
  ...nextVitals,
  ...nextTs,
  // Override default ignores of eslint-config-next.
  globalIgnores([
    // Default ignores of eslint-config-next:
    ".next/**",
    "out/**",
    "build/**",
    "next-env.d.ts",
    // Standalone Node scripts driven by scripts/test_ui.sh. They are plain
    // CommonJS run outside the Next.js bundle (Playwright is not an app
    // dependency), so the app lint rules do not apply to them.
    "tests/e2e/**",
  ]),
]);

export default eslintConfig;

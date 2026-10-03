const fs = require('fs');
const path = require('path');
const apiPath = path.join(process.env.HOME, 'Desktop/TMdemo/src/lib/api.ts');
let content = fs.readFileSync(apiPath, 'utf8');

const target = 'response = await fetch(path, {';
const replacement = `const API_BASE = import.meta.env.VITE_API_URL || "";
    response = await fetch(API_BASE + path, {`;

if (content.includes(target)) {
    content = content.replace(target, replacement);
    fs.writeFileSync(apiPath, content);
    console.log("Patched api.ts");
} else {
    console.log("Target string not found");
}

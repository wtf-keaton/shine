import { spawn } from "node:child_process";
import process from "node:process";

function run(cmd, args, opts = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(cmd, args, { stdio: "inherit", shell: true, ...opts });
    child.on("close", (code) => {
      if (code === 0) resolve();
      else reject(new Error(`${cmd} ${args.join(" ")} exited with code ${code}`));
    });
  });
}

const [, , subcommand = "help"] = process.argv;

if (subcommand === "build") {
  await run("npm", ["run", "shine:frontend:install"]);
  await run("npm", ["run", "shine:frontend:build"]);
  await run("npm", ["run", "shine:embed"]);
  await run("npm", ["run", "shine:cpp:build"]);
  process.exit(0);
}

console.log(`Unknown subcommand: ${subcommand}

Usage:
  npm run shine build
`);
process.exit(1);


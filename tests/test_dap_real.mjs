#!/usr/bin/env node
/* Real emu-debug integration for the frozen emuSA80535-DAP PR #4 client.
 * This test imports the DAP build but never changes DAP product sources. */

import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { PassThrough } from "node:stream";
import { pathToFileURL } from "node:url";

const FROZEN_DAP_HEAD = "36639b48ddb2ffbafa14c00da794fe1734f7483b";
const REQUIRED_CAPABILITIES = [
  "rawCode64k",
  "deterministicReset",
  "snapshotBasicRegisters",
  "decodeCode",
  "replaceCodeBreakpoints",
  "boundedRun",
  "stepInstruction",
];

function requireArgument(index, name) {
  const value = process.argv[index];
  if (value === undefined || value.length === 0) {
    throw new Error(`usage: test_dap_real.mjs DAP_ROOT EMU_DEBUG (missing ${name})`);
  }
  return path.resolve(value);
}

function git(root, args) {
  return execFileSync("git", ["-C", root, ...args], { encoding: "utf8" }).trim();
}

function isFile(file) {
  try {
    return fs.statSync(file).isFile();
  } catch {
    return false;
  }
}

function dapFrame(message) {
  const payload = Buffer.from(JSON.stringify(message), "utf8");
  return Buffer.concat([
    Buffer.from(`Content-Length: ${payload.length}\r\n\r\n`, "ascii"),
    payload,
  ]);
}

function dapReader(output) {
  let buffer = Buffer.alloc(0);
  const queued = [];
  const waiters = [];
  output.on("data", (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);
    for (;;) {
      const headerEnd = buffer.indexOf("\r\n\r\n");
      if (headerEnd < 0) return;
      const header = buffer.subarray(0, headerEnd).toString("ascii");
      const length = Number.parseInt(/Content-Length: (\d+)/i.exec(header)?.[1] ?? "", 10);
      const start = headerEnd + 4;
      if (!Number.isSafeInteger(length) || buffer.length < start + length) return;
      const message = JSON.parse(buffer.subarray(start, start + length).toString("utf8"));
      buffer = buffer.subarray(start + length);
      const waiter = waiters.shift();
      if (waiter === undefined) queued.push(message);
      else waiter(message);
    }
  });
  return async () => {
    const ready = queued.shift();
    if (ready !== undefined) return ready;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error("DAP smoke timeout")), 10_000);
      waiters.push((message) => {
        clearTimeout(timer);
        resolve(message);
      });
    });
  };
}

function assertDapSuccess(message, command) {
  assert.equal(message.type, "response");
  assert.equal(message.command, command);
  assert.equal(message.success, true, JSON.stringify(message));
}

function semanticSnapshot(snapshot) {
  return {
    state: snapshot.state,
    resultKind: snapshot.resultKind,
    reason: snapshot.reason,
    pc: snapshot.pc,
    a: snapshot.registers.a,
    b: snapshot.registers.b,
    sp: snapshot.registers.sp,
    dptr: snapshot.registers.dptr,
    variant: snapshot.variant,
    instructionCount: snapshot.instructionCount,
  };
}

async function launchReal(Backend, emulator, fixture) {
  const backend = new Backend({ commandTimeoutMs: 5_000, terminationTimeoutMs: 1_000 });
  const result = await backend.launch({
    program: fixture,
    entryAddress: 0,
    resetSeed: 525109,
    emulatorPath: emulator,
  });
  return { backend, result };
}

async function launchFake(Backend, dapRoot, fixture) {
  const fakeServer = path.join(
    dapRoot,
    "out",
    "test-fixtures",
    "fake-emulator",
    "server.js",
  );
  assert.ok(isFile(fakeServer), `DAP fake build missing: ${fakeServer}`);
  const backend = new Backend({
    childArguments: [fakeServer, "--headless-debug", "--scenario", "compatible"],
    commandTimeoutMs: 5_000,
    terminationTimeoutMs: 1_000,
  });
  const result = await backend.launch({
    program: fixture,
    entryAddress: 0,
    resetSeed: 525109,
    emulatorPath: process.execPath,
  });
  return { backend, result };
}

async function exercise(candidate) {
  const { backend, result } = candidate;
  assert.deepEqual(result.hello.protocol, { major: 1, minor: 0 });
  assert.deepEqual(result.hello.capabilities, REQUIRED_CAPABILITIES);
  assert.ok(result.hello.variants.includes("sab80535"));
  assert.equal(result.entrySnapshot.reason, "entry");
  assert.equal(result.entrySnapshot.pc, 0);

  const forward = await backend.decodeCode(0, 0, 0, 4);
  assert.deepEqual(
    forward.instructions.map(({ address, size, valid }) => ({ address, size, valid })),
    [
      { address: 0, size: 2, valid: true },
      { address: 2, size: 1, valid: true },
      { address: 3, size: 2, valid: true },
      { address: 5, size: 1, valid: true },
    ],
  );
  const replacement = await backend.replaceCodeBreakpoints([2]);
  assert.deepEqual(replacement.accepted, [2]);
  assert.deepEqual(replacement.rejected, []);
  const stopped = await backend.run(1_024);
  assert.equal(stopped.reason, "breakpoint");
  assert.equal(stopped.pc, 2);
  const stepped = await backend.stepInstruction();
  assert.equal(stepped.reason, "step");
  assert.equal(stepped.pc, 3);
  assert.equal(stepped.registers.a, 2);
  assert.deepEqual(await backend.getState(), stepped);
  const predecessor = await backend.decodeCode(3, 0, -1, 2);
  assert.deepEqual(
    predecessor.instructions.map(({ address, size, valid }) => ({ address, size, valid })),
    [
      { address: 2, size: 1, valid: true },
      { address: 3, size: 2, valid: true },
    ],
  );
  await backend.replaceCodeBreakpoints([]);
  const yielded = await backend.run(1);
  assert.equal(yielded.resultKind, "yield");
  assert.equal(yielded.reason, "yield");
  assert.equal(yielded.pc, 2);
  return {
    semantic: {
      launch: semanticSnapshot(result.entrySnapshot),
      stopped: semanticSnapshot(stopped),
      stepped: semanticSnapshot(stepped),
      yielded: semanticSnapshot(yielded),
    },
    machineCycles: [
      result.entrySnapshot.machineCycleCount,
      stopped.machineCycleCount,
      stepped.machineCycleCount,
      yielded.machineCycleCount,
    ],
    psw: [
      result.entrySnapshot.registers.psw,
      stopped.registers.psw,
      stepped.registers.psw,
      yielded.registers.psw,
    ],
  };
}

async function runDapSmoke(EmuDebugSession, EmulatorLaunchBackend, emulator, fixture) {
  const backend = new EmulatorLaunchBackend({
    commandTimeoutMs: 5_000,
    terminationTimeoutMs: 1_000,
  });
  const input = new PassThrough();
  const output = new PassThrough();
  const next = dapReader(output);
  let sequence = 0;
  const send = (command, arguments_) => {
    sequence += 1;
    input.write(dapFrame({
      seq: sequence,
      type: "request",
      command,
      arguments: arguments_,
    }));
  };
  new EmuDebugSession(undefined, true, backend).start(input, output);
  try {
    send("initialize", {
      adapterID: "emuSA80535",
      linesStartAt1: true,
      columnsStartAt1: true,
      pathFormat: "path",
    });
    assertDapSuccess(await next(), "initialize");
    send("launch", {
      type: "emuSA80535",
      request: "launch",
      name: "real emu-debug contract smoke",
      program: fixture,
      emulatorPath: emulator,
      entryAddress: "0x0000",
      resetSeed: 525109,
      stopOnEntry: true,
      trace: "off",
    });
    const initialized = await next();
    assert.equal(initialized.type, "event");
    assert.equal(initialized.event, "initialized");
    send("setInstructionBreakpoints", {
      breakpoints: [{ instructionReference: "code:0002" }],
    });
    const breakpointResponse = await next();
    assertDapSuccess(breakpointResponse, "setInstructionBreakpoints");
    assert.equal(breakpointResponse.body.breakpoints[0].verified, true);
    send("configurationDone", {});
    assertDapSuccess(await next(), "configurationDone");
    assertDapSuccess(await next(), "launch");
    const entry = await next();
    assert.deepEqual(entry.body, { reason: "entry", threadId: 1 });

    send("disassemble", {
      memoryReference: "code:0000",
      offset: 0,
      instructionOffset: 0,
      instructionCount: 3,
    });
    const disassembly = await next();
    assertDapSuccess(disassembly, "disassemble");
    assert.deepEqual(
      disassembly.body.instructions.map(({ address, presentationHint }) => ({
        address,
        presentationHint,
      })),
      [
        { address: "0x0000", presentationHint: "normal" },
        { address: "0x0002", presentationHint: "normal" },
        { address: "0x0003", presentationHint: "normal" },
      ],
    );
    assert.ok(disassembly.body.instructions.every(({ instruction }) =>
      typeof instruction === "string" && instruction.length > 0));

    send("continue", { threadId: 1 });
    assertDapSuccess(await next(), "continue");
    const breakpointStop = await next();
    assert.deepEqual(breakpointStop.body, {
      reason: "instruction breakpoint",
      threadId: 1,
    });
    send("stackTrace", { threadId: 1 });
    const stack = await next();
    assertDapSuccess(stack, "stackTrace");
    assert.equal(stack.body.stackFrames[0].instructionPointerReference, "code:0002");
    const frameId = stack.body.stackFrames[0].id;
    send("scopes", { frameId });
    const scopes = await next();
    assertDapSuccess(scopes, "scopes");
    send("variables", { variablesReference: scopes.body.scopes[0].variablesReference });
    const variables = await next();
    assertDapSuccess(variables, "variables");
    assert.deepEqual(
      variables.body.variables.slice(0, 2).map(({ name, value }) => [name, value]),
      [["PC", "0x0002"], ["A", "0x01"]],
    );

    send("stepIn", { threadId: 1, granularity: "instruction" });
    assertDapSuccess(await next(), "stepIn");
    const stepStop = await next();
    assert.deepEqual(stepStop.body, { reason: "step", threadId: 1 });
    send("setInstructionBreakpoints", { breakpoints: [] });
    assertDapSuccess(await next(), "setInstructionBreakpoints");
    send("continue", { threadId: 1 });
    assertDapSuccess(await next(), "continue");
    send("pause", { threadId: 1 });
    assertDapSuccess(await next(), "pause");
    const pauseStop = await next();
    assert.deepEqual(pauseStop.body, { reason: "pause", threadId: 1 });

    send("disconnect", {});
    assertDapSuccess(await next(), "disconnect");
    const terminated = await next();
    assert.equal(terminated.type, "event");
    assert.equal(terminated.event, "terminated");
  } finally {
    input.destroy();
    output.destroy();
    await backend.disconnect();
  }
}

async function main() {
  const dapRoot = requireArgument(2, "DAP_ROOT");
  const emulator = requireArgument(3, "EMU_DEBUG");
  assert.equal(git(dapRoot, ["rev-parse", "HEAD"]), FROZEN_DAP_HEAD);
  assert.equal(git(dapRoot, ["status", "--porcelain"]), "", "DAP worktree must stay clean");
  assert.ok(isFile(emulator), `runtime missing: ${emulator}`);
  const fixture = path.join(
    dapRoot,
    "test-fixtures",
    "firmware",
    "synthetic-loop.bin",
  );
  const clientModule = path.join(
    dapRoot,
    "out",
    "adapter",
    "src",
    "emulatorClient.js",
  );
  assert.ok(isFile(clientModule), `DAP build missing: ${clientModule}`);
  const { EmulatorLaunchBackend } = await import(pathToFileURL(clientModule).href);
  const sessionModule = path.join(dapRoot, "out", "adapter", "src", "session.js");
  assert.ok(isFile(sessionModule), `DAP session build missing: ${sessionModule}`);
  const { EmuDebugSession } = await import(pathToFileURL(sessionModule).href);

  const real = await launchReal(EmulatorLaunchBackend, emulator, fixture);
  const fake = await launchFake(EmulatorLaunchBackend, dapRoot, fixture);
  try {
    const realEvidence = await exercise(real);
    const fakeEvidence = await exercise(fake);
    /* The contract fake deliberately omits real parity updates and uses one
     * synthetic cycle per opcode. Compare the adapter-visible stop/control
     * semantics it claims, then assert the real core's accepted timing and
     * PSW behavior independently. */
    assert.deepEqual(realEvidence.semantic, fakeEvidence.semantic);
    assert.deepEqual(realEvidence.machineCycles, [0, 1, 2, 4]);
    assert.deepEqual(realEvidence.psw, [0, 1, 1, 1]);
    assert.match(real.result.hello.commit, /^[0-9a-f]{40}$/);
  } finally {
    await Promise.all([real.backend.disconnect(), fake.backend.disconnect()]);
  }
  await runDapSmoke(EmuDebugSession, EmulatorLaunchBackend, emulator, fixture);
  assert.equal(git(dapRoot, ["status", "--porcelain"]), "", "DAP sources changed");
  console.log(`DAP exact HEAD ${FROZEN_DAP_HEAD} real-contract/equivalence/F5 smoke passed`);
}

await main();

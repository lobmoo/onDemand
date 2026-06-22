#!/usr/bin/env node
"use strict";

const fs = require("fs");

const inputPath = process.argv[2];
const outputPath = process.argv[3];

if (!inputPath || !outputPath) {
  console.error("Usage: node ua-tour-analyze.js <input.json> <output.json>");
  process.exit(1);
}

let raw;
try {
  raw = fs.readFileSync(inputPath, "utf-8");
} catch (e) {
  console.error("Cannot read input file:", e.message);
  process.exit(1);
}

let data;
try {
  data = JSON.parse(raw);
} catch (e) {
  console.error("Invalid JSON:", e.message);
  process.exit(1);
}

const { nodes = [], edges = [], layers = [] } = data;

// Build adjacency maps
const fanIn = {};   // nodeId -> count of incoming edges
const fanOut = {};  // nodeId -> count of outgoing edges
const inEdges = {};  // nodeId -> [sourceIds]
const outEdges = {}; // nodeId -> [targetIds]

for (const n of nodes) {
  fanIn[n.id] = 0;
  fanOut[n.id] = 0;
  inEdges[n.id] = [];
  outEdges[n.id] = [];
}

for (const e of edges) {
  if (fanIn[e.target] !== undefined) fanIn[e.target]++;
  if (fanOut[e.source] !== undefined) fanOut[e.source]++;
  if (inEdges[e.target]) inEdges[e.target].push(e.source);
  if (outEdges[e.source]) outEdges[e.source].push(e.target);
}

// A. Fan-In Ranking (top 20)
const fanInRanking = nodes
  .map(n => ({ id: n.id, fanIn: fanIn[n.id] || 0, name: n.name }))
  .sort((a, b) => b.fanIn - a.fanIn)
  .slice(0, 20);

// B. Fan-Out Ranking (top 20)
const fanOutRanking = nodes
  .map(n => ({ id: n.id, fanOut: fanOut[n.id] || 0, name: n.name }))
  .sort((a, b) => b.fanOut - a.fanOut)
  .slice(0, 20);

// C. Entry Point Candidates
const entryFileNames = [
  "index.ts", "index.js", "main.ts", "main.js", "app.ts", "app.js",
  "server.ts", "server.js", "mod.rs", "main.go", "main.py", "main.rs",
  "manage.py", "app.py", "wsgi.py", "asgi.py", "run.py", "__main__.py",
  "Application.java", "Main.java", "Program.cs", "config.ru", "index.php",
  "App.swift", "Application.kt", "main.cpp", "main.c", "main.cc"
];

const fanOutValues = nodes.map(n => fanOut[n.id] || 0).filter(v => v > 0);
const fanOutP90 = fanOutValues.length > 0
  ? fanOutValues.slice().sort((a, b) => a - b)[Math.floor(fanOutValues.length * 0.9)]
  : 0;
const fanInValues = nodes.map(n => fanIn[n.id] || 0);
const fanInP25 = fanInValues.length > 0
  ? fanInValues.slice().sort((a, b) => a - b)[Math.floor(fanInValues.length * 0.25)]
  : 0;

function scoreNode(n) {
  let score = 0;
  const fp = n.filePath || "";
  const name = n.name || "";

  if (n.type === "file") {
    if (entryFileNames.includes(name)) score += 3;
    const depth = fp.split("/").length;
    if (depth <= 2) score += 1;
    if ((fanOut[n.id] || 0) >= fanOutP90) score += 1;
    if ((fanIn[n.id] || 0) <= fanInP25) score += 1;
  }

  if (n.type === "document") {
    if (name.toLowerCase() === "readme.md" && !fp.includes("/")) score += 5;
    else if (name.endsWith(".md") && !fp.includes("/")) score += 2;
  }

  return score;
}

const entryPointCandidates = nodes
  .map(n => ({ id: n.id, score: scoreNode(n), name: n.name, summary: n.summary || "" }))
  .filter(e => e.score > 0)
  .sort((a, b) => b.score - a.score)
  .slice(0, 5);

// D. BFS from top code entry point
const importCallTypes = new Set(["imports", "calls", "includes", "requires", "uses"]);
const codeEntry = entryPointCandidates.find(e => e.id.startsWith("file:"));
const bfsStartNode = codeEntry ? codeEntry.id : (nodes.find(n => n.type === "file") || {}).id;

let bfsTraversal = { startNode: bfsStartNode, order: [], depthMap: {}, byDepth: {} };

if (bfsStartNode) {
  const visited = new Set();
  const queue = [{ id: bfsStartNode, depth: 0 }];
  visited.add(bfsStartNode);

  while (queue.length > 0) {
    const { id, depth } = queue.shift();
    bfsTraversal.order.push(id);
    bfsTraversal.depthMap[id] = depth;
    if (!bfsTraversal.byDepth[depth]) bfsTraversal.byDepth[depth] = [];
    bfsTraversal.byDepth[depth].push(id);

    const neighbors = outEdges[id] || [];
    for (const nb of neighbors) {
      if (!visited.has(nb)) {
        visited.add(nb);
        queue.push({ id: nb, depth: depth + 1 });
      }
    }
  }
}

// E. Non-Code File Inventory
const nonCodeFiles = { documentation: [], infrastructure: [], data: [], config: [] };

for (const n of nodes) {
  const entry = { id: n.id, name: n.name, summary: n.summary || "" };
  if (n.type === "document") {
    nonCodeFiles.documentation.push(entry);
  } else if (n.type === "service" || n.type === "pipeline" || n.type === "resource") {
    nonCodeFiles.infrastructure.push(entry);
  } else if (n.type === "table" || n.type === "schema") {
    nonCodeFiles.data.push(entry);
  } else if (n.type === "config") {
    nonCodeFiles.config.push(entry);
  }
}

// F. Tightly Coupled Clusters
const bidirectionalPairs = [];
for (const e of edges) {
  if (e.source === e.target) continue;
  const reverse = edges.find(r => r.source === e.target && r.target === e.source);
  if (reverse) {
    const pair = [e.source, e.target].sort();
    const key = pair.join("|");
    if (!bidirectionalPairs.find(p => p.key === key)) {
      bidirectionalPairs.push({ key, nodes: pair, count: 1 });
    } else {
      bidirectionalPairs.find(p => p.key === key).count++;
    }
  }
}

// Deduplicate
const pairMap = {};
for (const e of edges) {
  if (e.source === e.target) continue;
  const pair = [e.source, e.target].sort();
  const key = pair.join("|");
  if (!pairMap[key]) pairMap[key] = { nodes: pair, edgeCount: 0 };
  pairMap[key].edgeCount++;
}

// Find bidirectional pairs (edge count >= 2 means both directions)
const clusters = [];
const usedPairs = [];

for (const [key, val] of Object.entries(pairMap)) {
  if (val.edgeCount >= 2) {
    usedPairs.push(val);
  }
}

// Expand clusters: group pairs that share nodes
const clusterSets = [];
for (const p of usedPairs) {
  let merged = false;
  for (const cs of clusterSets) {
    if (p.nodes.some(n => cs.has(n))) {
      p.nodes.forEach(n => cs.add(n));
      merged = true;
      break;
    }
  }
  if (!merged) {
    const s = new Set(p.nodes);
    clusterSets.push(s);
  }
}

// Limit cluster sizes and format
for (const cs of clusterSets) {
  const arr = [...cs].slice(0, 5);
  if (arr.length >= 2) {
    let edgeCount = 0;
    for (const e of edges) {
      if (arr.includes(e.source) && arr.includes(e.target)) edgeCount++;
    }
    clusters.push({ nodes: arr, edgeCount });
  }
}

// Sort by edgeCount descending, take top 10
clusters.sort((a, b) => b.edgeCount - a.edgeCount);
const topClusters = clusters.slice(0, 10);

// G. Layer List
const layerList = {
  count: layers.length,
  list: layers.map(l => ({ id: l.id, name: l.name, description: l.description }))
};

// H. Node Summary Index
const nodeSummaryIndex = {};
for (const n of nodes) {
  nodeSummaryIndex[n.id] = { name: n.name, type: n.type, summary: n.summary || "" };
}

// Output
const result = {
  scriptCompleted: true,
  entryPointCandidates,
  fanInRanking,
  fanOutRanking,
  bfsTraversal,
  nonCodeFiles,
  clusters: topClusters,
  layers: layerList,
  nodeSummaryIndex,
  totalNodes: nodes.length,
  totalEdges: edges.length
};

try {
  fs.writeFileSync(outputPath, JSON.stringify(result, null, 2), "utf-8");
  console.log("Analysis complete. Output written to", outputPath);
} catch (e) {
  console.error("Cannot write output:", e.message);
  process.exit(1);
}

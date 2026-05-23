const fs = require('fs');
const path = require('path');

/*
  Metadata extractor (experimental)
  - Scans Source/ for MetaSound node metadata and writes docs/source/node_data.json.

  - This parser relies on simple regex and will miss nodes when metadata uses variable aliases 
    other than `Metadata` (for example `M` or `Meta`) or when declaration styles vary.
  - Update regex patterns before trusting extracted output!! :)
*/

const ROOT_DIR = path.resolve(__dirname, '../../Source');
const TARGET_EXTENSIONS = ['.cpp', '.h'];
const OUTPUT_FILE = path.resolve(__dirname, 'node_data.json');

const METASOUND_PARAM_REGEX = /METASOUND_PARAM\s*\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\)\s*;/g;
const IO_VERTEX_REGEX = /\bT(Input|Output)DataVertex\s*<\s*([\w:]+)\s*>\s*\(\s*METASOUND_GET_PARAM_NAME_AND_METADATA\s*\(\s*(\w+)\s*\)\s*\)/g;
// Support common metadata variable aliases used across files.
const META_VAR = '(?:Metadata|Meta|M)';

const CLASS_NAME_REGEX = new RegExp(`${META_VAR}\\.ClassName\\s*=\\s*\\{\\s*([\\s\\S]*?)\\s*\\}\\s*;`);
const MAJOR_VERSION_REGEX = new RegExp(`${META_VAR}\\.MajorVersion\\s*=\\s*(\\d+)\\s*;`);
const MINOR_VERSION_REGEX = new RegExp(`${META_VAR}\\.MinorVersion\\s*=\\s*(\\d+)\\s*;`);
const AUTHOR_REGEX = new RegExp(`${META_VAR}\\.Author\\s*=\\s*(?:TEXT\\s*\\(\\s*"([^"]+)"\\s*\\)|"([^"]+)")\\s*;`);
const DISPLAY_NAME_REGEX = new RegExp(`${META_VAR}\\.DisplayName\\s*=\\s*(?:\\w*LOCTEXT\\w*)\\s*\\(\\s*"[^"]+"\\s*,\\s*"([^"]+)"\\s*\\)\\s*;`);
const DESCRIPTION_REGEX = new RegExp(`${META_VAR}\\.Description\\s*=\\s*(?:\\w*LOCTEXT\\w*)\\s*\\(\\s*"[^"]+"\\s*,\\s*"([^"]+)"\\s*\\)\\s*;`);
const CATEGORY_REGEX = new RegExp(`${META_VAR}\\.CategoryHierarchy\\s*=\\s*\\{\\s*([\\s\\S]*?)\\s*\\}\\s*;`);

function getAllMatches(regex, content) {
  let matches = [];
  let match;
  while ((match = regex.exec(content)) !== null) {
    matches.push(match);
  }
  return matches;
}

function parseClassNameTextBlock(textBlock) {
  const pattern = /TEXT\s*\(\s*"([^"]+)"\s*\)/g;
  let results = [];
  let match;
  while ((match = pattern.exec(textBlock)) !== null) {
    results.push(match[1]);
  }
  return results;
}

function guessParamType(cppType) {
  if (cppType === 'bool') return 'Bool';
  if (cppType === 'FTime') return 'Time';
  if (cppType === 'FAudioBuffer') return 'Audio';
  return cppType;
}

function parseSource(filePath) {
  const content = fs.readFileSync(filePath, 'utf8');

  const result = {
    file: filePath,
    nodes: []
  };

  const classNameMatch = CLASS_NAME_REGEX.exec(content);
  if (!classNameMatch) {
    return [];
  }

  let nodeData = {
    name: '',
    description: '',
    className: '',
    majorVersion: '',
    minorVersion: '',
    author: '',
    category: [],
    inputs: [],
    outputs: []
  };

  const textBlock = classNameMatch[1];
  const segments = parseClassNameTextBlock(textBlock);
  nodeData.className = segments.join(' ');

  let majorMatch = MAJOR_VERSION_REGEX.exec(content);
  if (majorMatch) nodeData.majorVersion = majorMatch[1];

  let minorMatch = MINOR_VERSION_REGEX.exec(content);
  if (minorMatch) nodeData.minorVersion = minorMatch[1];

  let authorMatch = AUTHOR_REGEX.exec(content);
  if (authorMatch) nodeData.author = authorMatch[1] || authorMatch[2];

  let displayNameMatch = DISPLAY_NAME_REGEX.exec(content);
  if (displayNameMatch) nodeData.name = displayNameMatch[1];

  let descMatch = DESCRIPTION_REGEX.exec(content);
  if (descMatch) nodeData.description = descMatch[1];

  let categoryMatch = CATEGORY_REGEX.exec(content);
  if (categoryMatch) {
    nodeData.category = parseClassNameTextBlock(categoryMatch[1]);
  }

  const paramMatches = getAllMatches(METASOUND_PARAM_REGEX, content);
  
  const paramMap = {}; 
  paramMatches.forEach(m => {
    const symbolName = m[1];
    const displayName = m[2];
    const description = m[3];
    paramMap[symbolName] = {
      name: displayName,
      description: description,
      type: 'Audio'
    };
  });

  const ioMatches = getAllMatches(IO_VERTEX_REGEX, content);
  ioMatches.forEach(m => {
    let direction = m[1];
    let cType = m[2];
    let symbol = m[3];
    let shortType = guessParamType(cType);

    if (paramMap[symbol]) {
      paramMap[symbol].type = shortType;
    } else {
      paramMap[symbol] = {
        name: symbol,
        description: "",
        type: shortType
      };
    }
  });

  const directionMap = {};
  ioMatches.forEach(m => {
    const direction = m[1];
    const symbol = m[3];
    directionMap[symbol] = direction;
  });

  for (let symbolName in paramMap) {
    let paramInfo = paramMap[symbolName];
    let dir = directionMap[symbolName] || 'Input';
    if (dir === 'Input') {
      nodeData.inputs.push({
        name: paramInfo.name,
        description: paramInfo.description,
        type: paramInfo.type
      });
    } else {
      nodeData.outputs.push({
        name: paramInfo.name,
        description: paramInfo.description,
        type: paramInfo.type
      });
    }
  }

  result.nodes.push(nodeData);
  return [result];
}

function scanDirectory(dir) {
  let allNodes = [];
  const entries = fs.readdirSync(dir);

  for (const entry of entries) {
    const fullPath = path.join(dir, entry);
    const stat = fs.statSync(fullPath);

    if (stat.isDirectory()) {
      allNodes = allNodes.concat(scanDirectory(fullPath));
    } else {
      const ext = path.extname(entry);
      if (TARGET_EXTENSIONS.includes(ext)) {
        const nodesInFile = parseSource(fullPath);
        if (nodesInFile.length > 0) {
          allNodes = allNodes.concat(nodesInFile);
        }
      }
    }
  }
  return allNodes;
}

const allParsed = scanDirectory(ROOT_DIR);

const finalOutput = [];
for (const fileObj of allParsed) {
  for (const nodeObj of fileObj.nodes) {
    finalOutput.push({
      ...nodeObj 
    });
  }
}

fs.writeFileSync(OUTPUT_FILE, JSON.stringify(finalOutput, null, 2), 'utf8');
console.log(`Done! Created: ${OUTPUT_FILE}`);
const fs = require('fs');
const path = require('path');

const projectRoot = '../../'; 
const metaSoundNodes = [];

const vertexRegex = /METASOUND_GET_PARAM_NAME(?:_AND_METADATA)?\s*\(\s*([\w\d_]+)\s*(?:,\s*([\w\d_"\s]+))?\)/g;
const nodeMetadataRegex = /FNodeClassMetadata\s+([\w\d_]+)\s*=\s*{([\s\S]*?)}/g;

function scanDirectory(dir) {
    const files = fs.readdirSync(dir);
    for (const file of files) {
        const filePath = path.join(dir, file);
        const stat = fs.statSync(filePath);
        if (stat.isDirectory()) {
            scanDirectory(filePath);
        } else if (file.endsWith('.h') || file.endsWith('.cpp')) {
            processFile(filePath);
        }
    }
}

function processFile(filePath) {
    const content = fs.readFileSync(filePath, 'utf8');

    let match;
    let node = { file: filePath, vertices: [] };

    while ((match = vertexRegex.exec(content)) !== null) {
        node.vertices.push({
            name: match[1],
            description: match[2] ? match[2].replace(/["]/g, '') : ''
        });
    }

    while ((match = nodeMetadataRegex.exec(content)) !== null) {
        node.metadata = match[2].trim();
    }

    if (node.vertices.length > 0 || node.metadata) {
        metaSoundNodes.push(node);
    }
}

scanDirectory(path.join(projectRoot, 'Source'));

fs.writeFileSync('node_data.json', JSON.stringify(metaSoundNodes, null, 2), 'utf8');

console.log('Done.');
const fs = require('fs');
const path = require('path');

const inputFile = path.join(__dirname, 'nodes.json');
const outputDir = path.join(__dirname, '../');

if (!fs.existsSync(outputDir)) {
  fs.mkdirSync(outputDir);
}

const data = JSON.parse(fs.readFileSync(inputFile, 'utf8'));

const sidebarContent = data.map((d, i) => {
    const safeName = d.name.replace(/\s+/g, '').replace(':Array', '');
    const nodeFileName = `${safeName}.html`;
    const space = i > 0 ? '            ' : '';
    return `${space}<li><a href="${nodeFileName}">${d.name}</a></li>`;
}).join('\n');

data.forEach(node => {
  const { name, description, image, inputs, outputs, category } = node;

  const inputRows = inputs.map(input => {
    return `
      <tr>
        <td>${input.name}</td>
        <td>${input.description}</td>
        <td>${input.type}</td>
      </tr>
    `;
  }).join('\n');

  const outputRows = outputs.map(output => {
    return `
      <tr>
        <td>${output.name}</td>
        <td>${output.description}</td>
        <td>${output.type}</td>
      </tr>
    `;
  }).join('\n');

  const htmlContent = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>${name}</title>
  <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="X-UA-Compatible" content="ie=edge">
  <link rel="stylesheet" href="./style.css">
</head>
<body>
    <nav class="sidebar">
        <ul>
            ${sidebarContent}
        </ul>
    </nav>
  <main>
    <h1><a href="https://matthewscharles.github.io/metasound-branches/">MetaSound Branches</a></h1>
    <h2>${name}</h2>
    <p><strong>Category:</strong> ${category}</p>
    <p>${description}</p>
    <img src="./svg/${image}" alt="${name}">
    <h3>Inputs</h3>
    <table>
      <thead>
        <tr>
          <th>Name</th>
          <th>Description</th>
          <th>Type</th>
        </tr>
      </thead>
      <tbody>
        ${inputRows}
      </tbody>
    </table>

    <h3>Outputs</h3>
    <table>
      <thead>
        <tr>
          <th>Name</th>
          <th>Description</th>
          <th>Type</th>
        </tr>
      </thead>
      <tbody>
        ${outputRows}
      </tbody>
    </table>
    <br><br>
    <hr>
    <br><br>
    <a href="https://github.com/matthewscharles/">Charles Matthews 2025</a>
  </main>
  <button class="menu-toggle" aria-label="Toggle Menu">☰</button>
</body>

<script>
  document.addEventListener('DOMContentLoaded', function() {
    const sidebar = document.querySelector('.sidebar');
    const toggleBtn = document.querySelector('.menu-toggle');

    toggleBtn.addEventListener('click', () => {
      sidebar.classList.toggle('show');
    });
  });
</script>
</html>
`;

  const fileName = name.replace(/\s+/g, '').replace(':Array', '') + '.html';
  const filePath = path.join(outputDir, fileName);
  
  fs.writeFileSync(filePath, htmlContent, 'utf8');
  console.log(`- ${fileName}`);
});

const tableHeader = `| Node | Category | Description |
|------|-----------|-------------|
`;

const tableRows = data.map(d => {
  const safeName = d.name.replace(/\s+/g, '');
  const nodeURL = `https://matthewscharles.github.io/metasound-branches/${safeName}.html`;
  const category = d.category || '-';
  return `| [\`${d.name}\`](${nodeURL}) | ${category} | ${d.description} |`;
}).join('\n');

const mdContent = tableHeader + tableRows + '\n';

fs.writeFileSync(path.join(outputDir, 'nodes.md'), mdContent, 'utf8');
console.log('-----');
console.log('- nodes.md');

// data.forEach(node => {
//   const { name, description, image, inputs, outputs, category } = node;
//   console.log(`<img src="${image}" alt="${name}">`);
// })
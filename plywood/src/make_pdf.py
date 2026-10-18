"""PLAN.md -> PLAN.pdf (python-markdown + headless Chrome). Run from the plywood/ folder."""
import markdown, subprocess, os, shutil
here = os.getcwd()
html = markdown.markdown(open('PLAN.md').read(), extensions=['tables', 'fenced_code'])
css = """body{font-family:Helvetica,Arial,sans-serif;font-size:10.5pt;line-height:1.4;margin:0 6mm}
table{border-collapse:collapse;margin:6px 0;font-size:9pt}td,th{border:1px solid #bbb;padding:3px 6px;vertical-align:top}
th{background:#eee}img{max-width:100%;max-height:120mm;display:block;margin:6px auto}h1{font-size:20pt}h2{font-size:15pt;border-bottom:1px solid #ccc;page-break-after:avoid}
h3{page-break-after:avoid}code{background:#f3f3f3;padding:0 3px}tr{page-break-inside:avoid}"""
open('/tmp/PLAN.html', 'w').write(f'<html><head><meta charset="utf-8"><base href="file://{here}/"><style>{css}</style></head><body>{html}</body></html>')
chrome = shutil.which('google-chrome') or shutil.which('chromium')
subprocess.run([chrome, '--headless', '--no-sandbox', '--disable-gpu', '--allow-file-access-from-files', '--no-pdf-header-footer',
                f'--print-to-pdf={here}/PLAN.pdf', 'file:///tmp/PLAN.html'], check=True, capture_output=True)
print('PLAN.pdf written')

#!/usr/bin/env python3
"""Generate DOCX and PDF from docs/Guide.md for Raspberry Pi LED lab."""
from __future__ import annotations
import re
from pathlib import Path
from xml.sax.saxutils import escape as xml_escape

from docx import Document
from docx.shared import Cm, Pt
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Preformatted, KeepTogether

ROOT = Path(__file__).resolve().parent
INPUT = ROOT / "Guide.md"
OUTBASE = "RPi_LED_Character_Driver_Application_Lab_Guide"

def parse(src: str):
    blocks = []
    buf = []
    code = []
    in_code = False

    def flush():
        if buf:
            blocks.append(("para", " ".join(buf)))
            buf.clear()

    lines = src.splitlines()
    i = 0
    while i < len(lines):
        raw = lines[i]
        line = raw.rstrip()
        if line.startswith("```"):
            if in_code:
                blocks.append(("code", "\n".join(code)))
                code = []
                in_code = False
            else:
                flush()
                in_code = True
        elif in_code:
            code.append(line)
        elif not line.strip():
            flush()
        elif line.startswith("#"):
            flush()
            level = len(line) - len(line.lstrip("#"))
            blocks.append(("heading", level, line[level:].strip()))
        elif line.startswith(">"):
            flush()
            blocks.append(("quote", line[1:].strip()))
        elif line.startswith("- "):
            flush()
            blocks.append(("bullet", line[2:].strip()))
        elif line.startswith("|"):
            flush()
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                pieces = [v.strip() for v in lines[i].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-{2,}:?", x or "") for x in pieces):
                    rows.append(pieces)
                i += 1
            blocks.append(("table", rows))
            continue
        else:
            buf.append(line)
        i += 1
    flush()
    return blocks

def strip_md(s: str) -> str:
    s = s.replace("`", "").replace("**", "")
    s = re.sub(r"\[([^]]+)\]\(https?://[^)]+\)", r"\1", s)
    return s

def docx_blocks(blocks, output: Path):
    doc = Document()
    sec = doc.sections[0]
    sec.top_margin = Cm(2.1)
    sec.bottom_margin = Cm(1.9)
    sec.left_margin = sec.right_margin = Cm(2.1)
    for name, size, bold in [
        ("Normal", 10, False), ("Title", 19, True),
        ("Heading 1", 14, True), ("Heading 2", 11, True)
    ]:
        style = doc.styles[name]
        style.font.name = "NanumGothic"
        style.font.size = Pt(size)
        style.font.bold = bold
        el = style.element.get_or_add_rPr()
        rf = el.rFonts
        if rf is None:
            rf = OxmlElement("w:rFonts")
            el.insert(0, rf)
        rf.set(qn("w:eastAsia"), "NanumGothic")

    for b in blocks:
        t = b[0]
        if t == "heading":
            _, level, val = b
            if level == 1:
                doc.add_paragraph(strip_md(val), "Title")
            else:
                doc.add_heading(strip_md(val), level=min(level - 1, 2))
        elif t == "para":
            doc.add_paragraph(strip_md(b[1]))
        elif t == "quote":
            doc.add_paragraph(strip_md(b[1]), "Subtitle")
        elif t == "bullet":
            doc.add_paragraph(strip_md(b[1]), "List Bullet")
        elif t == "code":
            p = doc.add_paragraph()
            p.paragraph_format.space_before = Pt(3)
            p.paragraph_format.space_after = Pt(6)
            for line in b[1].splitlines():
                r = p.add_run(line + "\n")
                r.font.name = "Consolas"
                r.font.size = Pt(8)
        elif t == "table":
            rows = b[1]
            if not rows:
                continue
            cols = max(map(len, rows))
            tb = doc.add_table(rows=len(rows), cols=cols)
            tb.style = "Table Grid"
            for ri, row in enumerate(rows):
                for ci, cell in enumerate(row):
                    tb.cell(ri, ci).text = strip_md(cell)
                    if ri == 0:
                        for para in tb.cell(ri, ci).paragraphs:
                            for r in para.runs:
                                r.bold = True
    doc.save(output)

def find_font():
    options = [
        "/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
        "/usr/share/fonts/truetype/nanum/NanumGothicBold.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    ]
    for p in options[:2]:
        if Path(p).exists():
            return p
    raise FileNotFoundError("Install fonts-nanum for Korean PDF output")

def pdf_blocks(blocks, output: Path):
    pdfmetrics.registerFont(TTFont("NanumGothic", find_font()))
    normal = ParagraphStyle("Body", fontName="NanumGothic", fontSize=9.2, leading=14,
                             spaceAfter=7, wordWrap="CJK", alignment=TA_LEFT)
    h1 = ParagraphStyle("Title", parent=normal, fontSize=18, leading=23,
                        spaceBefore=8, spaceAfter=13)
    h2 = ParagraphStyle("Heading1", parent=normal, fontSize=13, leading=18,
                        spaceBefore=15, spaceAfter=8, keepWithNext=True)
    h3 = ParagraphStyle("Heading2", parent=normal, fontSize=11, leading=16,
                        spaceBefore=10, spaceAfter=5, keepWithNext=True)
    quote = ParagraphStyle("Quote", parent=normal, textColor=colors.HexColor("#3f5367"),
                           leftIndent=10, spaceBefore=5, spaceAfter=10)
    cell = ParagraphStyle("Cell", parent=normal, fontSize=8.2, leading=12,
                          spaceAfter=0)
    code_style = ParagraphStyle("Code", parent=normal, fontName="Courier",
                                fontSize=7.3, leading=10, leftIndent=4, spaceAfter=8)
    story = []
    for b in blocks:
        kind = b[0]
        if kind == "heading":
            story.append(Paragraph(xml_escape(strip_md(b[2])), {1:h1,2:h2}.get(b[1],h3)))
        elif kind == "para":
            story.append(Paragraph(xml_escape(strip_md(b[1])), normal))
        elif kind == "quote":
            story.append(Paragraph(xml_escape(strip_md(b[1])), quote))
        elif kind == "bullet":
            story.append(Paragraph("• " + xml_escape(strip_md(b[1])), normal))
        elif kind == "code":
            # Use each line as a separate paragraph to avoid clipping very long lines.
            story.append(Spacer(1, 3))
            for ln in b[1].splitlines():
                line = ln.replace(" ", "\u00a0")
                # Paragraph wraps long lines; Courier is used for ASCII snippets.
                story.append(Paragraph(xml_escape(line or " "), code_style))
            story.append(Spacer(1, 3))
        elif kind == "table" and b[1]:
            rows = b[1]
            ncols = max(map(len, rows))
            vals = [[Paragraph(xml_escape(strip_md(v)), cell) for v in row] +
                    [Paragraph("", cell)]*(ncols-len(row)) for row in rows]
            page_width = A4[0] - 94
            table = Table(vals, colWidths=[page_width/ncols]*ncols, repeatRows=1, hAlign="LEFT")
            table.setStyle(TableStyle([
                ("BACKGROUND",(0,0),(-1,0), colors.HexColor("#eff2f5")),
                ("GRID",(0,0),(-1,-1), 0.25, colors.HexColor("#bdc4cb")),
                ("VALIGN",(0,0),(-1,-1),"TOP"),
                ("LEFTPADDING",(0,0),(-1,-1),5),
                ("RIGHTPADDING",(0,0),(-1,-1),5),
                ("TOPPADDING",(0,0),(-1,-1),5),
                ("BOTTOMPADDING",(0,0),(-1,-1),5),
            ]))
            story.append(table)
            story.append(Spacer(1, 8))
    def header_footer(canvas, doc):
        canvas.saveState()
        canvas.setFont("NanumGothic", 8)
        canvas.drawString(47, A4[1] - 29, "Raspberry Pi LED Character Device Driver Lab")
        canvas.drawRightString(A4[0] - 47, 26, str(doc.page))
        canvas.restoreState()
    pdf = SimpleDocTemplate(str(output), pagesize=A4, leftMargin=47,
                            rightMargin=47, topMargin=47, bottomMargin=47)
    pdf.build(story, onFirstPage=header_footer, onLaterPages=header_footer)

if __name__ == "__main__":
    blocks = parse(INPUT.read_text(encoding="utf-8"))
    docx = ROOT / (OUTBASE + ".docx")
    pdf = ROOT / (OUTBASE + ".pdf")
    docx_blocks(blocks, docx)
    pdf_blocks(blocks, pdf)
    print(f"Created: {docx} ({docx.stat().st_size} bytes)")
    print(f"Created: {pdf} ({pdf.stat().st_size} bytes)")

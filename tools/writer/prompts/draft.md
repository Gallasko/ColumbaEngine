Write chapter {{n}} of an article for the ColumbaEngine blog, in the "{{voice}}" voice.

The voice:
{{voiceText}}

The chapter:
- title: {{title}}
- it answers: {{answers}}
- its core topics, explained in full: {{core}}
- its facts and numbers, with their sources: {{facts}}
- a visual it wants: {{visual}} (where it belongs, leave a line `<!-- visual: what it shows -->`;
  nothing when "none")

The whole plan:

{{plan}}

The chapters kept before this one (the reader has read them):

{{previous}}

The code for this chapter (quote what the chapter needs, in fenced blocks, keeping the
`file:lines` reference above each):

{{code}}

Use only the numbers and facts given above (the facts line, the code, the chapters before):
never invent a measurement, a percentage or a date. Where the chapter needs a number it does not have,
leave `<!-- number: what is needed -->` for the author.

Follow the teaching rules of the system prompt. Open with how this chapter answers what the
previous one left open (unless it is the first). Write the chapter's text only, in Markdown: no
chapter heading (the tool adds it), no "##" headings inside.

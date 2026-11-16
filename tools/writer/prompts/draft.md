Write chapter {{n}} of an article for the ColumbaEngine blog, in the "{{voice}}" voice.

The voice:
{{voiceText}}

The chapter:
- title: {{title}}
- it answers: {{answers}}
- its core topics, explained in full: {{core}}
- a visual it wants: {{visual}} (where it belongs, leave a line `<!-- visual: what it shows -->`;
  nothing when "none")

The whole plan:

{{plan}}

The chapters kept before this one (the reader has read them):

{{previous}}

The code for this chapter (quote what the chapter needs, in fenced blocks, keeping the
`file:lines` reference above each):

{{code}}

Follow the teaching rules of the system prompt. Open with how this chapter answers what the
previous one left open (unless it is the first). Write the chapter's text only, in Markdown: no
chapter heading (the tool adds it), no "##" headings inside.

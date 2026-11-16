Make a teaching plan for an article on the ColumbaEngine blog.

The idea:
- title: {{title}}
- question: {{question}}
- why it is worth reading: {{why}}
- sources:
{{sources}}

The engine's repository is {{engine}}; the author's notes are in {{notes}}. Read the sources (and
anything they lead to) before planning: the plan must rest on what the code and notes really say,
with the real numbers.

Follow the teaching rules of the system prompt. The plan gives:

- title: the article's title
- theme: one of {{themes}}
- reader: who the article is for (the default reader unless the subject asks for another)
- knows: what that reader already knows
- learns: what they know at the end
- question: the one question carried through the article
- chapters, in order, each one step of the answer, nothing used before it is explained:
  - title
  - answers: what this chapter answers, and how it follows from the previous one
  - core: the core topics it explains, comma separated (what only exists in ColumbaEngine or
    PgScript is always core; general programming never is, unless it is the subject)
  - code: the engine code it needs, said in words (a function, a file), or "none"
  - visual: a chart or diagram it wants, said in words, or "none"

Three to seven chapters. Every value on one line.

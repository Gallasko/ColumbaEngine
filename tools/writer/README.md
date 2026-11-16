# article — the writing tool for columbaengine.org

A command-line tool in PgScript that helps write the blog's articles: it finds ideas in recent
work, makes a teaching plan, drafts each chapter in several voices, pulls the engine code a chapter
needs, joins the kept chapters into the site, and prepares the Reddit and X posts. Claude drafts,
through `claude -p` on your subscription (no API key); it only reads and answers, the tool writes
every file.

## Setup

```bash
alias article='/home/corse/Documents/PgEngine/release/PgCompilerBootstrap /home/corse/Documents/PgEngine/tools/writer/article.pg'
```

The site is `/home/corse/Documents/AstroTest` (or `$ARTICLE_SITE`). The first run writes the
defaults into `<site>/writer/`, never over a file you changed:

| File | What it is |
|---|---|
| `config.json` | paths (claude, engine, notes, site), the author, the model of each step |
| `guide.md` | the teaching rules every step follows: edit it to tune the articles |
| `voices.md` | the voices, a `## name` section each; every chapter is drafted in each |
| `themes.json` | a theme → where its posts go |
| `questions.md` | paste questions people ask; the ideas step reads it |

## The steps, in order

```bash
article ideas                      # new ideas from your commits, notes, questions (Claude reads, read-only)
article ideas --again              # the last 60 days again, whatever the last scan covered (--days N for another window)
article ideas "about the VM"       # steer one run; it asks even when nothing changed
article ideas list                 # by status: new, planned, written, declined
article idea "My own idea"         # add one; article decline <id> drops one
article plan 3                     # writer/articles/<slug>/plan.md: edit it before drafting
article code <slug> 2              # code/02.md, the engine code chapter 2 needs (draft runs it when missing)
article draft <slug> 2             # chapters/02-<voice>.md in every voice (--again to redo)
article pick <slug> 2 story        # keep a draft as chapters/02.md (edit it freely after)
article mine <slug> 3              # or write chapters/03.md yourself and record it as yours
article stitch <slug>              # src/content/blog/<slug>.md in the site, and the checks of the thread
article share <slug>               # the posts, with ?from= tags, printed to paste
article status <slug>              # where the article stands
article state <slug>               # the same as JSON with the next step (the Writer app reads it)
```

Then commit and deploy the site yourself. `plan.md` is yours to edit at any time: add, remove or
rename chapters, change the reader; the next command follows it.

## What is recorded

`writer/articles/<slug>/record.json`: the idea, the reader, per chapter the drafts made, the voice
kept and how much you changed it (`edited`, 0 to 1), the checks, the posts and their tags. The tags
are what the site's analytics see in `?from=`, so every visit can be traced to its post, and every
article to how it was written. `writer/last-call.json` holds the last Claude call (prompt, answer),
to see what went wrong.

## Tests

```bash
cd /home/corse/Documents/PgEngine && release/PgCompilerBootstrap tools/writer/test/run.pg [group]
```

Groups: foundations, core, ideas, plan, draft, stitch, share. Each command runs against a fake
`claude` (`test/fake-claude.sh`, answering from `test/replies/<step>.json`) in temporary folders:
the tests never spend your usage or touch the real site.

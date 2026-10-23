# Chronicle: the Collegium rework (draft)

Status: draft for the author's review, 2026-10-23. Nothing is built.

## Why

From the author's playthrough of the mage arc:

- Training on the dummy happens on its own; it should be a fight you play.
- Choosing a course is a story card; it should replace the day's choices with the courses.
- "I was sent away and I don't even understand why." The ending should be two tasks you take,
  one after the other: the theory exam, then the practical exam.
- The theory should be real questions: four multiple-choice questions of four answers, where
  the more you studied and the more Intelligence you have, the more wrong answers are struck off.
- The practical exam is a real fight.
- Vitality should not come back during a story, so the last fight is harder for who did not
  rest before it.

## What the player sees

The term is 16 days for its 24 months, as now. Each day is 1 AP.

1. **Days 1 to 6, the first weeks.** As now: the elemental lectures, the familiar studies, the
   library, copying for the scriptorium, a day's rest. Each lecture has its fee at the door.
2. **Day 7, choosing a course.** The day's choices are replaced by the four courses, and nothing
   else is offered that day:

   | Course | Cost |
   |---|---|
   | The fire course | 1 AP |
   | The frost course | 1 AP |
   | The storm course | 1 AP |
   | All three: the elemental basics | 3 AP, or 2 AP with Intelligence 14 |

   The basics take the rest of day 7 and days 8 and 9 (or day 8). A student who has not heard the
   elemental lectures twice by then is offered the basics only (as now: no fork without them).
3. **Days 8 to 14, the chosen course.** As now: the course, the binding hall, the library, and
   practising on the dummies. **Practising on the dummies is a fight you play by hand**, on the
   fight screen, against one training dummy that hits back softly.
4. **Day 15, Take the theory exam.** A task of the day (the only one). Taking it opens four
   questions, one after the other, each with four answers. Before each question is shown, some
   of its wrong answers are struck off (crossed out, and cannot be chosen):

   | Your preparation | Wrong answers struck |
   |---|---|
   | under 10 | none |
   | 10 to 17 | one |
   | 18 or more | two |

   Preparation = Intelligence + twice the days in the library + the days of courses, plus what
   the childhood gave (the old tome, the forbidden shelf, Mara, Father Anselm's trust, the Old
   Chronicle, Letters 3): the same counts the theory reads today. The score is the right answers,
   0 to 4.
5. **Day 16, Take the practical exam.** The only task of the day: the fight against the two
   examination dummies, as now.
6. **The grade.** Pass with 2 right answers or more **and** the practical passed. Merit with 3,
   distinction with 4 (and the practical passed well). Failing either is failing the Collegium.
7. **No more "sent away".** The gate that sent you away on day 15 for too few courses is
   removed: you always reach the exams, and fewer courses means fewer answers struck off. The
   masters' standing line says how ready you are, in words.
8. **Vitality during a story.** It does not come back with the months while a story runs
   (the Collegium, the outskirts, the library). A day's rest (+10) is the only way to mend it
   inside the Collegium, and you start the practical with what you have left.

## The questions (to write with the author)

A pool of about 12 questions, 4 drawn for each exam, each with one right answer and three
wrong ones, in the game's world: the three elements and what they do to each other, the
familiars and what they eat, the town's history, the Collegium itself. Examples to react to:

- *Which element puts out fire?* Frost · Storm · Stone · Ash
- *What does a well imp ask to be paid in?* Coin · Rations · Berries · Nothing
- *Who burned his own book?* Saint Orun · Father Anselm · The old miller · Sergeant Edric
- *What does storm do to a target soaked with frost?* Shatters it · Heals it · Nothing · Doubles the frost

## Calls I made (the author to confirm)

- The struck-answer thresholds (10, 18) and Intelligence 14 for the cheaper basics.
- Pass at 2 of 4, merit 3, distinction 4.
- Removing the "sent away" gate.
- A day's rest stays the one way to mend inside a story.
- The question pool and its 4 draws a term (the same draws for a given life's seed).
- All the Collegium's text is rewritten in the player's voice at the same time.

## For the builder

- The story engine has tiles per day (`days`), costs in AP (`cost`), `onlyWith`/`"not"`,
  fights by hand (`fight` without `auto`), tests of kind `card` and `fight`. New: a day whose
  tiles replace all others (day 7), a tile cost that depends on a stat (the basics), a test made
  of several questions with answers struck by a score, and no mending while a story runs.
- Vitality: the month rule mends it (`resources.pg`'s `mending`); while a story runs the scene
  should tell the rules to skip it.

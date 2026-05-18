# The Tutoring Center Scheduling Problem (Bonus reformulation)

## The setting

A university department runs a tutoring center that supports a portfolio of courses across an entire semester. Every week, hundreds of students request help, and a mixed workforce of tutors must be matched to those requests. The scheduler is not a one-time exercise — it runs every night, all term long, and it has to behave well under all the messiness of real university life: students drop courses in the first two weeks, tutors get sick, exam periods spike demand, rooms get double-booked, and people lie about how badly they need help when they think it'll get them ahead in the queue.

The job is to produce, every night, a schedule for the next several days that respects every hard rule, balances a long list of softer goals, and is fair enough that nobody walks away feeling cheated.

## The people involved

There are three groups.

The first group is the **tutors**. They are not interchangeable. Some are undergraduate teaching assistants, paid a small stipend and capped at ten hours of tutoring per week. Some are graduate teaching assistants, paid more and capped at twenty. A few are paid hourly tutors hired from a small budget that the department cannot exceed. Each tutor has their own weekly availability — their own classes, their own labs, prayer times, family obligations — and they submit this availability at the start of the term. Each tutor is also only qualified to teach certain topics: a freshman undergrad TA can tutor introductory programming but not graduate-level algorithms; a graduate TA fluent in Arabic can cover bilingual sessions while another cannot. Some tutors have passed certain courses with strong grades and are therefore certified to tutor them; others are not. Tutors get tired: a tutor who has just done three back-to-back hour-long sessions is not going to do a fourth one well, and the center's policy reflects that.

The second group is the **students**. A student doesn't make a single lifetime request; they may ask for several sessions in the same week, on different topics, with their own preferences. Each student has their own course schedule, so any session they're assigned to must not clash with a class they're enrolled in. Each student declares whether they want one-on-one tutoring or are willing to join a small group, and what the largest group they'd tolerate is. Each student declares whether they want the session online, in person, or either. Some students have accessibility needs registered with the disability services office — captioning, accessible rooms, extra time — and their assignments are only feasible in slots that carry those accommodations. Some students fall into priority cohorts — students on academic warning because their GPA has dropped below the departmental threshold, and professional athletes whose training and competition schedules sharply constrain when they can attend — and the center has committed to giving these students a baseline level of service. Students rank their preferred sessions, but the rankings vary in length: some submit only one acceptable slot; others submit ten.

The third group is the **physical infrastructure**. Rooms have capacities and equipment. The circuits course needs a lab with oscilloscopes; the proof-heavy math course needs a whiteboard; an online session needs one of a finite number of Zoom licenses the department has paid for. Rooms have their own schedules and are shared with other departments. A tutor assigned to two consecutive sessions in two different buildings needs enough time to walk between them.

## What makes a schedule valid

A valid schedule never violates a hard rule. The hard rules are these.

No tutor is ever asked to be in two places at once. No tutor exceeds their weekly hour cap, their daily hour cap, or works past the limits of their own stated availability. No tutor is asked to teach a topic they aren't qualified for, in a language they don't speak, or in a course they haven't passed. No tutor is given so many back-to-back sessions that they don't have a short break. No room is double-booked or filled past its capacity. No equipment is conjured into existence — if a session needs a lab PC, the assigned room has one. No student is assigned to a session that overlaps with one of their own classes. No student is placed in a session that doesn't match their declared modality (online vs. in person) or that exceeds their declared maximum group size. No student with an accessibility flag is placed in a session that can't accommodate them. No student is placed in a slot they didn't list as acceptable. The total of paid tutoring hours, across the whole term, does not exceed the budget the department has been allocated.

## What makes a schedule good

Beyond legality, the schedule is judged on softer qualities, and the center has to decide how to weigh them.

The first goal is simply that as many students as possible get matched at all, and that as many as possible get one of their top-ranked preferences rather than a low-ranked one. The second goal is continuity: if a student has been seeing the same tutor on the same topic, they tend to do better in the course, so all else equal the system should keep them together. The third goal is the opposite of continuity at a different scale: across the term, no student should be funneled to the same tutor every single time, and no tutor should hoard the same student's hours, because both invite favoritism complaints. The fourth goal is curricular: if a student wants help on a topic that depends on an earlier one they haven't yet been tutored in, the system should gently steer them through the prerequisite first. The fifth goal is balance: tutors of the same rank should get roughly equal hours so nobody feels overworked or under-utilized. The sixth goal is to minimize tutor travel time between rooms, since wasted minutes hurt session quality. The seventh is to keep student wait times — the gap between submitting a request and getting helped — as short as possible, weighted by how urgent the request was.

## What makes the problem hard

The scheduling does not happen in a single batch. Requests come in continuously throughout the day, and students need answers quickly — they have to plan their week. So the scheduler effectively has to make commitments before it has seen all the requests it will eventually need to handle, and it has to do so without painting itself into a corner.

Tutors and students cancel. A student who books on Monday and cancels on Wednesday frees up a seat, and the scheduler should fill it — but only with someone who would actually benefit and won't be displaced by a further cancellation cascade. A tutor who calls in sick on the morning of a session forces a last-minute re-match, ideally to a tutor the affected students would also have been happy with.

People don't always show up. Each student has a historical no-show rate; if the rate is high enough, the center may want to gently overbook certain popular slots, accepting a small risk that everyone shows up and one student has to be turned away, in exchange for serving more students on average.

People also lie. If the form has an "urgency" field and a "first preference" field, students will quickly learn to mark everything urgent and to game their preference list strategically. The scheduling rule has to be designed so that the best thing a student can do is to report honestly — anything else should not help them.

Demand swings violently. The week before midterms looks nothing like the third week of the term. The schedule has to anticipate this and keep a buffer of unbooked tutor hours for walk-ins and emergencies during high-demand windows.

## What success looks like

A successful scheduler, running every night of the semester, produces a daily plan in which every hard rule above is satisfied; the soft goals are traded off in a way the department can explain to a complaining student or a complaining tutor; and the outcome remains good not just for the average request but across every cohort the center has committed to serve — including the students who are easiest to lose, who submit late, who don't speak up, and who would never write a complaint email if the system quietly let them down.

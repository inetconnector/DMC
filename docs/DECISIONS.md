# Design Decisions

D001 Block size = 64 by default.
Reason: good compromise between routing accuracy and memory locality.

D002 Route refresh = 8 decode tokens.
Reason: reduce routing overhead while preserving locality.

D003 Dense path remains default.
Reason: simplify validation and preserve backward compatibility.

D004 Persistent summaries.
Reason: avoid recomputing block statistics every decode step.

# Design Decisions

D001 Block size = 64 by default.
Reason: good compromise between locality and summary granularity.

D002 Summary refresh = 8 decode tokens.
Reason: keep fixed summaries reasonably fresh without making the path query-driven.

D003 Dense path remains default.
Reason: simplify validation and preserve backward compatibility.

D004 Persistent summaries.
Reason: avoid recomputing all multiresolution statistics every decode step.

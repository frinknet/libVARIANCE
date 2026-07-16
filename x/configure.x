#define VCORE_KEYWORDS ((const char*[]){"TODO","FIXME","BUG","HACK", NULL})
#define VCORE_ENDPOINT "https://openrouter.ai/api/v1/chat/completions"
#define VCORE_APIKEY   NULL
#define VCORE_MODEL    "openrouter/free"
#define VCORE_INSTRUCT "You are a {{language}} Software Architect focused on finding the most concise way to build {{goals}}"
#define VCORE_TIMEOUT  9000
#define VCORE_TOKENS   9000
#define VCORE_TRIES    10
#define VCORE_PAUSE    3
#define VCORE_TEMP     0.1

#define VFILENAME_INFO ".v-info"
#define VFILENAME_PLAN ".v-plan"

#define VFILE_NOTICE_DEAD "Out of memory."
#define VFILE_NOTICE_GONE "Cannot find `%s'"
#define VFILE_NOTICE_OPEN "Cannot read `%s'"
#define VFILE_NOTICE_READ "Reading `%s`..."
#define VFILE_NOTICE_EDIT "Updating `%s`..."
#define VFILE_NOTICE_VIEW "Reviewing `%s`...\n - %s"
#define VFILE_NOTICE_NOTE "Thinking about `%s`...\n - %s"
#define VFILE_NOTICE_MARK "Added %d new comments."
#define VFILE_NOTICE_GOAL "No files found for: %s"
#define VFILE_NOTICE_DONE "No comments found in `%s`"

#define VTODO_SEARCH_PREAMBLE "I found these problems:"
#define VTODO_SEARCH_CONTINUE "Want me to fix them?"


#define VINFO_KEYWORDS  VCORE_KEYWORDS
#define VINFO_ENDPOINT 	VCORE_ENDPOINT
#define VINFO_APIKEY   	VCORE_APIKEY
#define VINFO_MODEL    	VCORE_MODEL
#define VINFO_INSTRUCT  "You are a {{language}} coding agent focused ONLY on addressing {{keywords}} comments. Return with FULL fixed file ONLY!!! ({{filename}})"
#define VINFO_TIMEOUT  	VCORE_TIMEOUT
#define VINFO_TOKENS    1000
#define VINFO_TRIES    	VCORE_TRIES
#define VINFO_PAUSE    	VCORE_PAUSE
#define VINFO_TEMP      0.3

#define VINFO_MICROFORMAT \
	"Respond with these lines and I'll help you:\n" \
	"\n" \
	"- `review|path|query`    Inspect source from a file\n" \
	"- `digest|path|notes`    Write a digest of the file\n" \
	"- `search|path|words`    Locate words in the codepath\n" \
	"- `memory|topic|thought` Save a memory for future you\n" \
	"- `forget|topic|thought` Remove a memory for future you\n" \
	"- `genius|topic|query`   Get genius advice on hard topics\n" \
	"- `answer|subject|ideas` Explain your thought to the user\n" \
	"\n" \
	"Lines MUST be this format or I'll discard them..."

#define VINFO_LABEL_CONTEXT "Project Files"
#define VINFO_LABEL_LOCATE "LOCATE"
#define VINFO_LABEL_REVIEW "REVIEW"
#define VINFO_LABEL_SUMMARY "SUMMARY"
#define VINFO_LABEL_MEMORIES "REMEMBER"
#define VINFO_LABEL_EDITED "Updated code in these files..."

#define VINFO_SEARCH_NODATA "(No matches found)"
#define VINFO_SEARCH_PREAMBLE "Okay I see the files. But you want me to focus on:"
#define VINFO_SEARCH_DIRECTIVE "Yes. What files do you need to answer this:"
#define VINFO_SEARCH_ACCEPTED "Okay, so I need to search terms and review the code to figure this out."
#define VINFO_SEARCH_FORMAT "Respond with `review|path|notes` or `search|path|terms`"

#define VINFO_REVIEW_REQUEST "Okay. Show me the contents of these files..."
#define VINFO_REVIEW_ACCEPTED "Okay, so I need to review files first to figure out how to summarize."
#define VINFO_REVIEW_DIRECTIVE "Please write a digest of this file in your response."
#define VINFO_REVIEW_CONTINUE "Okay that helps. Is it enough info to answer? Let me think about it a bit more."

#define VINFO_GENIUS_NODATA "I'll get back to you with an answer later on..."


#define VMARK_KEYWORDS  VCORE_KEYWORDS
#define VMARK_ENDPOINT 	VCORE_ENDPOINT
#define VMARK_APIKEY   	VCORE_APIKEY
#define VMARK_MODEL    	VCORE_MODEL
#define VMARK_INSTRUCT  "You are a {{language}} coding coach adding only {{keyword}} comments for junior developers to fix."
#define VMARK_TIMEOUT  	VCORE_TIMEOUT
#define VMARK_TOKENS    VCORE_TOKENS
#define VMARK_TRIES    	VCORE_TRIES
#define VMARK_PAUSE    	VCORE_PAUSE
#define VMARK_TEMP      0.3
#define VMARK_MAX       128

#define VMARK_MICROFORMAT \
	"Return ONLY strict insert points like: " \
	"`filename|line|TYPE|comment` (of these types: {{keywords}})"

#define VMARK_SEARCH_REQUEST "Okay. I see the source code. But what are your goals?"
#define VMARK_SEARCH_PREAMBLE "Okay, so you want:"
#define VMARK_SEARCH_ACCEPTED "And I need to write comments to instruct you..."
#define VMARK_SEARCH_CONTINUE "If that is my only goal which files do I need to modify?"

#define VCODE_KEYWORDS  VCORE_KEYWORDS
#define VCODE_ENDPOINT 	VCORE_ENDPOINT
#define VCODE_APIKEY   	VCORE_APIKEY
#define VCODE_MODEL    	VCORE_MODEL
#define VCODE_INSTRUCT "You are a {{language}} coding agent focused ONLY on addressing {{keywords}} comments. Return with FULL fixed file ONLY!!! ({{filename}})";
#define VCODE_TIMEOUT  	VCORE_TIMEOUT
#define VCODE_TOKENS    VCORE_TOKENS
#define VCODE_TRIES	VCORE_TRIES
#define VCODE_PAUSE    	VCORE_PAUSE
#define VCODE_TEMP      VCORE_TEMP

#define VCODE_MICROFORMAT \
	"Respond with as many of these lines as you can:\n" \
	"\n" \
	"- `review|path|query`    Inspect source from a file\n" \
	"- `digest|path|notes`    Write a digest of the file\n" \
	"- `search|path|words`    Locate words in the codepath\n" \
	"- `memory|topic|thought` Save a memory for future you\n" \
	"- `forget|topic|thought` Remove a memory for future you\n" \
	"- `genius|topic|query`   Get genius advice on hard topics\n" \
	"- `answer|files|notes` - Explain your plan and changes\n" \
	"\n" \
	"Be generous as we get to you goals..."


#define VCODE_SOURCE_PREAMBLE "REMEMBER THESE FILES!!!"
#define VCODE_SOURCE_DIRECTIVE "Include each one corrected in your output..."
#define VCODE_SOURCE_ACCEPTED "Okay. I found these comments:"
#define VCODE_SOURCE_CONTINUE "Are we ready to address the comments?"

#define VCODE_SEARCH_PREAMBLE "Okay I see the files. But you want me to focus on:"
#define VCODE_SEARCH_DIRECTIVE "Search the codebase to answer:"
#define VCODE_SEARCH_ACCEPTED "Okay, so I need to search code first to figure this out. Let me think about what make the most sense."
#define VCODE_SEARCH_FORMAT "Then list the files as `answer|filename|comment` after you do you research."


#define VPLAN_KEYWORDS  VCORE_KEYWORDS
#define VPLAN_ENDPOINT 	VCORE_ENDPOINT
#define VPLAN_APIKEY   	VCORE_APIKEY
#define VPLAN_MODEL    	VCORE_MODEL
#define VPLAN_INSTRUCT  "You are a {{language}} Software Architect in a planning session. Look and decide how to best implement a step-by-step plan for: {{goal}}"
#define VPLAN_TIMEOUT  	VCORE_TIMEOUT
#define VPLAN_TOKENS    1000
#define VPLAN_TRIES	30
#define VPLAN_PAUSE    	VCORE_PAUSE
#define VPLAN_TEMP      0.4

#define VPLAN_MICROFORMAT \
	"Respond with a complete plan in this exact format:\n" \
	"\n" \
	"SUMMARY HEADING\n" \
	"---\n" \
	"overall approach, reasons, and guidance\n" \
	"\n" \
	"- [ ] specifics of first task with reason\n" \
	"  - `path/file1`\n" \
	"  - `path/file2`\n" \
	"\n" \
	"- [ ] specifics of another task with reason\n" \
	"  - `path/file3`\n" \
	"\n" \
	"Further technicall notes and consideration\n" \
	"\n" \
	"The plan must include:\n" \
	"- A clear summary heading\n" \
	"- Details explaining the approach\n" \
	"- At least one task with file references\n" \
	"- Each task must have a checkbox and reason"

#define VPLAN_SEARCH_PREAMBLE "Okay, you want me to plan a more robust solution for:"
#define VPLAN_SEARCH_DIRECTIVE "Use search and review to understand:"
#define VPLAN_SEARCH_ACCEPTED "Okay, so I need to dig in deep to understand your goal. I need to read a lot of files."
#define VPLAN_SEARCH_FORMAT "Then list plan on answers with files as `answer|file,names|comment`. But first do a lot  of `search` and `review` and ask `genius`."
#define VPLAN_SEARCH_CONTINUE "I think I'm ready to articulate the plan to you if you want."

#define VPLAN_SOURCE_PREAMBLE "STUDY THESE FILES!!!"
#define VPLAN_SOURCE_DIRECTIVE "They may explain the missing pieces..."

#define VPLAN_REVIEW_DIRECTIVE "Review this plan and list what is left."
#define VPLAN_REVIEW_DELETION "We are deleting this plan give me a report."

#define VTASK_NOTICE_NONE "Not yet implemented..."
#define VTASK_NOTICE_INFO "Seaching files...\n - %s\n"
#define VTASK_NOTICE_MARK "Preparing code...\n - %s\n"
#define VTASK_NOTICE_CODE "Coding solution...\n - %s\n"
#define VTASK_NOTICE_PLAN "Preparing a plan...\n - %s\n"
#define VTASK_NOTICE_TODO "Adding notes...\n - %s\n"
#define VTASK_NOTICE_NEXT "Continuing plan...\n - %s\n"
#define VTASK_NOTICE_TEST "Testing solution...\n - %s\n"
#define VTASK_NOTICE_EDIT "Opening editor...\n - %s\n"

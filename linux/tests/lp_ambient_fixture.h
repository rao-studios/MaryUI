/* What maryd would answer the Ambient app (ambient.state, trace.list), so it renders and tests the same on
 * every machine: TextEdit leading with a document and a selection, the Media Player co-active, and two turns
 * in the trace — a question about the document, and a chat. */
#ifndef LP_AMBIENT_FIXTURE_H
#define LP_AMBIENT_FIXTURE_H

static const char LP_AMBIENT_STATE_LINE[] =
    "{\"type\":\"ambient\",\"state\":{\"places\":["
    "{\"place\":{\"token\":\"applications:textedit\",\"memoryToken\":\"textedit\",\"attention\":\"applications\",\"application\":\"textedit\",\"name\":\"TextEdit\",\"class\":\"workspace\",\"hasEyes\":true,\"focus\":\"writing\",\"order\":1000},"
    "\"surface\":{\"application\":{\"name\":\"TextEdit\",\"id\":\"textedit\",\"pid\":0},\"activeWindow\":{\"title\":\"Tides\"},\"windowCount\":1,\"minimizedCount\":0,"
    "\"elements\":[{\"identity\":\"textfield|Tides\",\"ordinal\":0,\"role\":\"textfield\",\"kind\":\"text field\",\"label\":\"Tides\",\"focused\":false,\"enabled\":true},"
    "{\"identity\":\"textarea|Document\",\"ordinal\":1,\"role\":\"textarea\",\"kind\":\"text area\",\"label\":\"Document\",\"focused\":true,\"enabled\":true},"
    "{\"identity\":\"button|Save\",\"ordinal\":2,\"role\":\"button\",\"kind\":\"button\",\"label\":\"Save\",\"focused\":false,\"enabled\":true}],"
    "\"focused\":{\"identity\":\"textarea|Document\",\"ordinal\":1,\"role\":\"textarea\",\"kind\":\"text area\",\"label\":\"Document\",\"focused\":true,\"enabled\":true},"
    "\"pageNotYetRead\":false,\"document\":\"/home/mary/Documents/Tides.txt\",\"capturedAt\":1757700000000,\"age\":3,\"fresh\":true,"
    "\"surfaceLine\":\"On screen: TextEdit \xE2\x80\x94 \\\"Tides\\\" (focused: Document) \xE2\x80\x94 offering: Document, Tides, Save \xE2\x80\x94 seen 3s ago\"},"
    "\"facts\":[{\"key\":\"applications:textedit/file\",\"slot\":\"file\",\"content\":\"The tide comes in twice a day, and the moon pulls it.\",\"subject\":\"Tides\",\"lower\":0,\"upper\":52,\"total\":52,"
    "\"provenance\":\"liveAX\",\"registration\":\"perceived\",\"capturedAt\":1757700000000,\"age\":3,\"fresh\":true,"
    "\"mention\":\"TextEdit \xC2\xB7 Tides \xE2\x80\x94 the document in front of them, characters 0\xE2\x80\x93" "52 of 52, seen 3s ago.\"}],"
    "\"isLead\":true,\"isCoActive\":false,\"isGlanced\":false},"
    "{\"place\":{\"token\":\"applications:media\",\"memoryToken\":\"media\",\"attention\":\"applications\",\"application\":\"media\",\"name\":\"Media Player\",\"class\":\"workspace\",\"hasEyes\":true,\"focus\":\"multimedia\",\"order\":1004},"
    "\"surface\":{\"application\":{\"name\":\"Media Player\",\"id\":\"media\",\"pid\":0},\"activeWindow\":{\"title\":\"Moonlight Sonata.mp3\"},\"windowCount\":1,\"minimizedCount\":0,\"elements\":[],\"pageNotYetRead\":false,"
    "\"capturedAt\":1757699994000,\"age\":9,\"fresh\":true,\"surfaceLine\":\"On screen: Media Player \xE2\x80\x94 \\\"Moonlight Sonata.mp3\\\" (focused: Pause) \xE2\x80\x94 offering: Pause, Previous, Next, Position, Volume \xE2\x80\x94 seen 9s ago\"},"
    "\"facts\":[{\"key\":\"applications:media/file\",\"slot\":\"file\",\"content\":\"playing, 2 of 12 in the folder, volume 60%\",\"subject\":\"Moonlight Sonata.mp3\",\"provenance\":\"liveAX\",\"registration\":\"perceived\","
    "\"capturedAt\":1757699994000,\"age\":9,\"fresh\":false,\"mention\":\"Media Player \xC2\xB7 Moonlight Sonata.mp3 \xE2\x80\x94 the document in front of them, seen 9s ago, so it may have moved on since.\"}],"
    "\"isLead\":false,\"isCoActive\":true,\"isGlanced\":false}],"
    "\"lead\":{\"token\":\"applications:textedit\",\"name\":\"TextEdit\",\"class\":\"workspace\"},"
    "\"selection\":{\"place\":{\"token\":\"applications:textedit\",\"name\":\"TextEdit\"},\"applicationID\":\"textedit\",\"text\":\"twice a day\",\"subject\":\"Tides\",\"capturedAt\":1757700001000,\"age\":2},"
    "\"utterance\":\"what does the document say about the tide?\",\"factCount\":2}}\n";

#define LP_AMBIENT_ROUTE_ASK \
    "{\"intent\":\"ask\",\"decidedBy\":\"namedPart\",\"verdicts\":{\"actionTurn\":false,\"namedPart\":\"tide\",\"namesAmbientSource\":false,\"isDeictic\":false,\"namesTransform\":false},"\
    "\"gate\":{\"questions\":[\"what\"],\"requestedAbilities\":[],\"applications\":[],\"memory\":{\"lanes\":[\"personal\"],\"abilityTargets\":[],\"lanePriority\":[\"personal\"],"\
    "\"relationshipHints\":[\"contains\",\"describes\",\"is\"],\"expandDisciplineUsage\":false,\"storageLanes\":[\"personal\",\"conversation\"]},\"semanticProjection\":\"what does the document say about the tide? [relationships: contains, describes, is]\"},"\
    "\"world\":{\"sense\":\"workspace\",\"attention\":\"applications\",\"subject\":\"Tides\",\"applicationID\":\"textedit\",\"fresh\":true},\"selectionDefinesTurn\":false,"\
    "\"leadApplicationID\":\"textedit\",\"leadPlace\":{\"token\":\"applications:textedit\",\"name\":\"TextEdit\",\"class\":\"workspace\",\"hasEyes\":true,\"focus\":\"writing\"},\"namedPlaces\":[],"\
    "\"realm\":{\"need\":{\"abilities\":[]},\"candidates\":["\
    "{\"place\":{\"token\":\"applications:calendar\",\"name\":\"Calendar\"},\"conformsByAbilities\":[],\"conformsByDiscipline\":false,\"targetClasses\":[],\"hasEyes\":true,\"conforms\":false},"\
    "{\"place\":{\"token\":\"applications:media\",\"name\":\"Media Player\"},\"conformsByAbilities\":[],\"conformsByDiscipline\":false,\"targetClasses\":[],\"hasEyes\":true,\"evidence\":\"activation\",\"evidenceAgeSeconds\":41,\"conforms\":false},"\
    "{\"place\":{\"token\":\"applications:textedit\",\"name\":\"TextEdit\"},\"conformsByAbilities\":[],\"conformsByDiscipline\":false,\"targetClasses\":[],\"hasEyes\":true,\"evidence\":\"activation\",\"evidenceAgeSeconds\":3,\"conforms\":false}],"\
    "\"place\":{\"token\":\"applications:textedit\",\"name\":\"TextEdit\"},\"decidedBy\":\"namedPart\"},"\
    "\"candidateAttentions\":[\"applications\",\"mac\",\"system\",\"window-management\",\"typer\"],\"needsLocate\":false,\"needsPreRead\":true,\"needsExecution\":true,\"rankingMode\":\"relevance\",\"isActionTurn\":false}"

#define LP_AMBIENT_ROUTE_CHAT \
    "{\"intent\":\"converse\",\"decidedBy\":\"none\",\"verdicts\":{\"actionTurn\":false,\"namesAmbientSource\":false,\"isDeictic\":false,\"namesTransform\":false},"\
    "\"gate\":{\"questions\":[\"how\"],\"requestedAbilities\":[],\"applications\":[],\"memory\":{\"lanes\":[\"personal\"],\"abilityTargets\":[],\"lanePriority\":[\"personal\"],\"relationshipHints\":[\"operates\",\"supports\",\"workflow\"],"\
    "\"expandDisciplineUsage\":false,\"storageLanes\":[\"personal\",\"conversation\"]},\"semanticProjection\":\"how was your day [relationships: operates, supports, workflow]\"},"\
    "\"selectionDefinesTurn\":false,\"namedPlaces\":[],\"realm\":{\"need\":{\"abilities\":[]},\"candidates\":[],\"decidedBy\":\"none\"},"\
    "\"candidateAttentions\":[\"applications\",\"mac\",\"system\",\"window-management\",\"typer\"],\"needsLocate\":false,\"needsPreRead\":false,\"needsExecution\":false,\"rankingMode\":\"relevance\",\"isActionTurn\":false}"

static const char LP_AMBIENT_TRACE_LINE[] =
    "{\"type\":\"trace\",\"records\":["
    "{\"id\":\"mary-turn-1757700003000-a1b2\",\"date\":1757700003000,\"age\":12,\"utterance\":\"what does the document say about the tide?\",\"route\":" LP_AMBIENT_ROUTE_ASK ","
    "\"systemPromptChars\":4180,\"exposedSkillCount\":0,\"packageIDs\":[\"textedit\",\"finder\",\"preview\",\"calendar\",\"media\",\"terminal\",\"settings\",\"calculator\"],"
    "\"skillRuns\":[{\"app\":\"textedit\",\"skill\":\"read\",\"invocation\":\"textedit.read\",\"status\":\"completed\",\"effect\":\"read\",\"started\":1757700003200,\"finished\":1757700003260,\"foundNothing\":false,\"args\":\"{\\\"about\\\":\\\"tide\\\"}\",\"result\":\"{\\\"passage\\\":\\\"The tide comes in twice a day\\\"}\"}],"
    "\"coActivePlaces\":[{\"token\":\"applications:media\",\"name\":\"Media Player\"}],\"glancedPlaces\":[],"
    "\"retrieval\":[{\"name\":\"context\",\"lanes\":[\"personal\",\"conversation\"],\"returned\":[{\"document_id\":\"file-2b3c4d5e6f7a8b9c\",\"group_id\":\"files-mary\",\"family\":\"file\",\"lane\":\"personal\",\"score\":4.21},"
    "{\"document_id\":\"mary-turn-1757699000000-c3d4\",\"group_id\":\"conversation-mary\",\"family\":\"conversation\",\"lane\":\"conversation\",\"score\":3.02}]}],"
    "\"contribution\":\"1 owner, 2 documents, 3 passages\"},"
    "{\"id\":\"mary-turn-1757699900000-e5f6\",\"date\":1757699900000,\"age\":115,\"utterance\":\"how was your day\",\"route\":" LP_AMBIENT_ROUTE_CHAT ","
    "\"systemPromptChars\":2311,\"exposedSkillCount\":0,\"packageIDs\":[],\"skillRuns\":[],\"coActivePlaces\":[],\"glancedPlaces\":[],"
    "\"retrieval\":[{\"name\":\"context\",\"lanes\":[\"personal\",\"conversation\"],\"returned\":[],\"warning\":\"asked nothing back\"}]}]}\n";

#endif

@ Returning payloads. The shared entry/event runner owns context and callbacks.
BattleScript_KoankoArceusEntry::
    callnative BS_KoankoArceusCheckOpening
    .4byte BattleScript_KoankoArceusEntryEnd
    setentrytrainerslidemessage KoankoArceusText_Remember
    trainerslidein BS_SCRIPTING
    waitstate
    printstring STRINGID_TRAINERSLIDE
    waitmessage B_WAIT_TIME_LONG
    flushtextbox
    trainerslideout BS_SCRIPTING
    waitstate
    handletrainerslidemsg BS_SCRIPTING, RESTORE_BATTLER_SLIDE_CONTROL
    callnative BS_KoankoArceusShowBugSpace
    waitabilitypopup
BattleScript_KoankoArceusEntryEnd:
    return

BattleScript_KoankoArceusOvercharge::
    setentrytrainerslidemessage KoankoArceusText_DismissClock
    trainerslidein BS_SCRIPTING
    waitstate
    printstring STRINGID_TRAINERSLIDE
    waitmessage B_WAIT_TIME_LONG
    flushtextbox
    callnative BS_KoankoArceusDismissClock
    .4byte BattleScript_KoankoArceusOverchargeEnd
    callnative BS_WaitBugSpacePanelSlide
    .byte BS_SCRIPTING
    setentrytrainerslidemessage KoankoArceusText_FairTrade
    printstring STRINGID_TRAINERSLIDE
    waitmessage B_WAIT_TIME_LONG
    flushtextbox
    setentrytrainerslidemessage KoankoArceusText_CommandSeal
    printstring STRINGID_TRAINERSLIDE
    waitmessage B_WAIT_TIME_LONG
    flushtextbox
    callnative BS_KoankoArceusHeal
    healthbarupdate BS_SCRIPTING
    datahpupdate BS_SCRIPTING, ASSURANCE_IGNORE
    waitstate
    callnative BS_KoankoArceusGrantBar
    .4byte BattleScript_KoankoArceusOverchargeEnd
    animatesavedbreakbar BS_SCRIPTING
    callnative BS_KoankoArceusFinishOvercharge
    .4byte BattleScript_KoankoArceusOverchargeEnd
    setentrytrainerslidemessage KoankoArceusText_FinalTrial
    printstring STRINGID_TRAINERSLIDE
    waitmessage B_WAIT_TIME_LONG
    flushtextbox
BattleScript_KoankoArceusOverchargeEnd:
    trainerslideout BS_SCRIPTING
    waitstate
    handletrainerslidemsg BS_SCRIPTING, RESTORE_BATTLER_SLIDE_CONTROL
    return

@ Restoration has already revealed the saved HP. The existing form animation
@ mosaics out, loads the actual species/palette at its peak, then reveals it.
BattleScript_KoankoArceusAdapt::
    callnative BS_KoankoArceusAdapt
    .4byte BattleScript_KoankoArceusAdaptEnd
    .4byte BattleScript_KoankoArceusAdaptReveal
    handleformchange BS_SCRIPTING, 0, FALSE
    waitstate
    playanimation BS_SCRIPTING, B_ANIM_FORM_CHANGE
    waitanimation
BattleScript_KoankoArceusAdaptReveal:
    handleformchange BS_SCRIPTING, 1, FALSE
    callnative BS_KoankoArceusFinishAdapt
BattleScript_KoankoArceusAdaptEnd:
    return

KoankoArceusText_Remember:
    .string "You remember this one, right?$"
KoankoArceusText_DismissClock:
    .string "Oh, right. That silly thing?\nWe won't be needing that.\pI'll keep the buff crushing though.$"
KoankoArceusText_FairTrade:
    .string "There! Much better!\nNow then, let's make this\ltrade fair...$"
KoankoArceusText_CommandSeal:
    .string "By my Command Seal!\nArceus, overcharge!\lRelease your shackles!$"
KoankoArceusText_FinalTrial:
    .string "Behold, your final trial!$"

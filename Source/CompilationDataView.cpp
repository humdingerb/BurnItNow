/*
 * Copyright 2010-2017, BurnItNow Team. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#include "BurnApplication.h"
#include "CompilationDataView.h"
#include "CommandThread.h"
#include "Constants.h"
#include "DirRefFilter.h"
#include "FolderSizeCount.h"

#include <Alert.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <Path.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>

#include <stdio.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Data view"


CompilationDataView::CompilationDataView(BurnWindow& parent)
	:
	BView(B_TRANSLATE("Data disc"), B_WILL_DRAW,
		new BGroupLayout(B_VERTICAL, kControlPadding)),
	fOpenPanel(NULL),
	fBurnerThread(NULL),
	fDirPath(new BPath()),
	fImagePath(new BPath()),
	fFolderSize(0),
	fNotification(B_PROGRESS_NOTIFICATION),
	fProgress(0),
	fETAtime("--"),
	fParser(fProgress, fETAtime)
{
	windowParent = &parent;
	step = NONE;

//	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	fBurnerInfoBox = new BSeparatorView(B_HORIZONTAL, B_FANCY_BORDER);
	fBurnerInfoBox->SetFont(be_bold_font);
	fBurnerInfoBox->SetLabel(B_TRANSLATE_COMMENT("Choose the folder to burn",
		"Status notification"));

	fPathView = new PathView("FolderStringView",
		B_TRANSLATE("Folder: <none>"));
	fPathView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	fPathView->SetExplicitMinSize(BSize(
		be_plain_font->StringWidth("A really fairly long disc label goes here"),
		B_SIZE_UNSET));

	fDiscLabel = new BTextControl("disclabel", B_TRANSLATE("Disc label:"), "",
		NULL);
	fDiscLabel->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));

	fBurnerInfoTextView = new BTextView("DataInfoTextView");
	fBurnerInfoTextView->SetWordWrap(false);
	fBurnerInfoTextView->MakeEditable(false);
	BScrollView* infoScrollView = new BScrollView("DataInfoScrollView",
		fBurnerInfoTextView, B_WILL_DRAW, true, true);
	infoScrollView->SetExplicitMinSize(BSize(B_SIZE_UNSET, 64));

	fChooseButton = new BButton("ChooseDirectoryButton",
		B_TRANSLATE("Choose folder"),
		new BMessage(kChooseMessage));
	fChooseButton->SetTarget(this);

	fImageButton = new BButton("BuildImageButton", B_TRANSLATE("Build image"),
		new BMessage(kBuildImageMessage));
	fImageButton->SetTarget(this);

	fBurnButton = new BButton("BurnImageButton", B_TRANSLATE("Burn disc"),
		new BMessage(kBurnDiscMessage));
	fBurnButton->SetTarget(this);

	fSizeView = new SizeView();

	BLayoutBuilder::Group<>(dynamic_cast<BGroupLayout*>(GetLayout()))
		.SetInsets(kControlPadding)
		.AddGrid(kControlPadding, 0, 0)
			.Add(fDiscLabel, 0, 0)
			.Add(fPathView, 0, 1)
			.Add(fChooseButton, 1, 0)
			.Add(fImageButton, 2, 0)
			.Add(fBurnButton, 3, 0)
			.SetColumnWeight(0, 10.f)
			.End()
		.AddGroup(B_VERTICAL)
			.Add(fBurnerInfoBox)
			.Add(infoScrollView)
			.End()
		.Add(fSizeView);

	_UpdateSizeBar();
}


CompilationDataView::~CompilationDataView()
{
	delete fBurnerThread;
	delete fOpenPanel;
}


#pragma mark -- BView Overrides --


void
CompilationDataView::AttachedToWindow()
{
	BView::AttachedToWindow();

	fChooseButton->SetTarget(this);
	fChooseButton->SetEnabled(true);

	fImageButton->SetTarget(this);
	fImageButton->SetEnabled(false);

	fBurnButton->SetTarget(this);
	fBurnButton->SetEnabled(false);
}


void
CompilationDataView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kChooseMessage:
			_ChooseDirectory();
			break;
		case kBurnDiscMessage:
			BurnDisc();
			break;
		case kBuildImageMessage:
			BuildISO();
			break;
		case B_REFS_RECEIVED:
		{
			_OpenDirectory(message);
			_GetFolderSize();
			break;
		}
		case kSetFolderSize:
		{
			message->FindInt64("foldersize", &fFolderSize);
			_UpdateSizeBar();
		}
		case kBurnerMessage:
			_BurnerOutput(message);
			break;
		default:
			BView::MessageReceived(message);
	}
}


#pragma mark -- Private Methods --


void
CompilationDataView::_ChooseDirectory()
{
	if (fOpenPanel == NULL) {
		fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), NULL,
			B_DIRECTORY_NODE, false, NULL, new DirRefFilter(), true);
		fOpenPanel->Window()->SetTitle(B_TRANSLATE("Choose data folder"));
	}
	fOpenPanel->Show();
}


void
CompilationDataView::_GetFolderSize()
{
	BMessage* msg = new BMessage('NULL');
	msg->AddString("path", fDirPath->Path());
	msg->AddMessenger("from", this);

	thread_id sizecount = spawn_thread(FolderSizeCount,
		"Folder size counter", B_LOW_PRIORITY, msg);

	if (sizecount >= B_OK)
		resume_thread(sizecount);

	fSizeView->ShowInfoText("calculating" B_UTF8_ELLIPSIS);
}


void
CompilationDataView::_OpenDirectory(BMessage* message)
{
	entry_ref ref;
	if (message->FindRef("refs", &ref) != B_OK)
		return;

	BEntry entry(&ref, true);	// also accept symlinks
	BNode node(&entry);
	if ((node.InitCheck() != B_OK) || !node.IsDirectory())
		return;

	fDirPath->SetTo(&entry);
	fPathView->SetText(fDirPath->Path());

	if (fDiscLabel->TextView()->TextLength() == 0) {
		fDiscLabel->SetText(fDirPath->Leaf());
		fDiscLabel->MakeFocus(true);
	}

	fImageButton->SetEnabled(true);
	fBurnButton->SetEnabled(false);
	fBurnerInfoBox->SetLabel(B_TRANSLATE_COMMENT("Build the image",
		"Status notification"));

//	fNotification.SetGroup("BurnItNow");
//	fNotification.SetMessageID("BurnItNow_Data");
//	fNotification.SetTitle(B_TRANSLATE("Building data image"));
//	fNotification.SetContent(B_TRANSLATE("Preparing the build" B_UTF8_ELLIPSIS));
//	fNotification.SetProgress(0);
//	 // It may take a while for the building to start...
//	fNotification.Send(60 * 1000000LL);
//
//		BString text = fBurnerInfoTextView->Text();
//		BString test("Track 01:  0 of  295 MB written (fifo 100%) [buf 100%]   4.0x.");
//		int32 modified = fParser.ParseLine(text, test);
//			_UpdateProgress();
//			fBurnerInfoTextView->SetText(text);
//			fBurnerInfoTextView->ScrollTo(0.0, 1000000.0);
//printf("percentage: %f, ETA: %s\n", fProgress, fETAtime.String());
//		snooze(4 * 1000000LL);	// wait 4 secs
//		test = "Track 01:  50 of  295 MB written (fifo 100%) [buf 100%]   4.0x.";
//		modified = fParser.ParseLine(text, test);
//			_UpdateProgress();
//			fBurnerInfoTextView->SetText(text);
//			fBurnerInfoTextView->ScrollTo(0.0, 1000000.0);
//printf("percentage: %f, ETA: %s\n", fProgress, fETAtime.String());
//		snooze(4 * 1000000LL);	// wait 4 secs
//		 test = "Track 01:  100 of  295 MB written (fifo 100%) [buf 100%]   4.0x.";
//		 modified = fParser.ParseLine(text, test);
//			_UpdateProgress();
//			fBurnerInfoTextView->SetText(text);
//			fBurnerInfoTextView->ScrollTo(0.0, 1000000.0);
//printf("percentage: %f, ETA: %s\n", fProgress, fETAtime.String());
//		snooze(4 * 1000000LL);	// wait 4 secs
//		 test ="Track 01:  150 of  295 MB written (fifo 100%) [buf 100%]   4.0x.";
//		 modified = fParser.ParseLine(text, test);
//			_UpdateProgress();
//			fBurnerInfoTextView->SetText(text);
//			fBurnerInfoTextView->ScrollTo(0.0, 1000000.0);
//printf("percentage: %f, ETA: %s\n", fProgress, fETAtime.String());
//		snooze(4 * 1000000LL);	// wait 4 secs
//		 test = "Track 01:  200 of  295 MB written (fifo 100%) [buf 100%]   4.0x.";
//		 modified = fParser.ParseLine(text, test);
//			_UpdateProgress();
//			fBurnerInfoTextView->SetText(text);
//			fBurnerInfoTextView->ScrollTo(0.0, 1000000.0);
//printf("percentage: %f, ETA: %s\n", fProgress, fETAtime.String());
//		snooze(4 * 1000000LL);	// wait 4 secs
//		 test = "Track 01:  250 of  295 MB written (fifo 100%) [buf 100%]   4.0x.";
//		 modified = fParser.ParseLine(text, test);
//			_UpdateProgress();
//			fBurnerInfoTextView->SetText(text);
//			fBurnerInfoTextView->ScrollTo(0.0, 1000000.0);
//printf("percentage: %f, ETA: %s\n", fProgress, fETAtime.String());
//		snooze(2 * 1000000LL);	// wait 4 secs
//		 test = "Track 01:  270 of  295 MB written (fifo 100%) [buf 100%]   4.0x.";
//		 modified = fParser.ParseLine(text, test);
//			_UpdateProgress();
//			fBurnerInfoTextView->SetText(text);
//			fBurnerInfoTextView->ScrollTo(0.0, 1000000.0);
//printf("percentage: %f, ETA: %s\n", fProgress, fETAtime.String());
//		snooze(2 * 1000000LL);	// wait 4 secs
//		 test = "Track 01:  295 of  295 MB written (fifo 100%) [buf 100%]   4.0x.";
//		 modified = fParser.ParseLine(text, test);
//			_UpdateProgress();
//			fBurnerInfoTextView->SetText(text);
//			fBurnerInfoTextView->ScrollTo(0.0, 1000000.0);
//printf("percentage: %f, ETA: %s\n", fProgress, fETAtime.String());
}


void
CompilationDataView::_BurnerOutput(BMessage* message)
{
	BString data;

	if (message->FindString("line", &data) == B_OK) {
		BString text = fBurnerInfoTextView->Text();
		int32 modified = fParser.ParseLine(text, data);
		if (modified == NOCHANGE) {
			data << "\n";
			fBurnerInfoTextView->Insert(data.String());
			fBurnerInfoTextView->ScrollBy(0.0, 50.0);
		} else {
			if (modified == PERCENT)
				_UpdateProgress();
			fBurnerInfoTextView->SetText(text);
			fBurnerInfoTextView->ScrollTo(0.0, 1000000.0);
		}
	}
	int32 code = -1;
	if ((message->FindInt32("thread_exit", &code) == B_OK)
			&& (step == BUILDING)) {
		fBurnerInfoBox->SetLabel(B_TRANSLATE_COMMENT("Burn the disc",
			"Status notification"));
		fImageButton->SetEnabled(false);
		fBurnButton->SetEnabled(true);

		fNotification.SetMessageID("BurnItNow_Data");
		fNotification.SetProgress(100);
		fNotification.SetContent(B_TRANSLATE("Building finished!"));
		fNotification.Send(5 * 1000000);	// 5 seconds

		step = NONE;

	} else if ((message->FindInt32("thread_exit", &code) == B_OK)
			&& (step == BURNING)) {
		fBurnerInfoBox->SetLabel(B_TRANSLATE_COMMENT(
			"Burning complete. Burn another disc?", "Status notification"));
		fChooseButton->SetEnabled(true);
		fImageButton->SetEnabled(false);
		fBurnButton->SetEnabled(true);

		fNotification.SetMessageID("BurnItNow_Data");
		fNotification.SetProgress(100);
		fNotification.SetContent(B_TRANSLATE("Burning finished!"));
		fNotification.Send(5 * 1000000);	// 5 seconds

		step = NONE;
		fParser.Reset();
	}
}


void
CompilationDataView::_UpdateProgress()
{
	if (fProgress == 0 || fProgress == 1.0)
		fNotification.SetContent(" ");
	else
		fNotification.SetContent(fETAtime);
	fNotification.SetMessageID("BurnItNow_Data");
	fNotification.SetProgress(fProgress);
	fNotification.Send();
}

void
CompilationDataView::_UpdateSizeBar()
{
	fSizeView->UpdateSizeDisplay(fFolderSize, DATA, CD_OR_DVD); // size in KiB
}


#pragma mark -- Public Methods --


void
CompilationDataView::BuildISO()
{
	if (fDirPath->Path() == NULL) {
		(new BAlert("ChooseDirectoryFirstAlert",
			B_TRANSLATE("First choose the folder to burn."),
			B_TRANSLATE("OK")))->Go();
		return;
	}
	if (fDirPath->InitCheck() != B_OK)
		return;

	if (fBurnerThread != NULL)
		delete fBurnerThread;

	fBurnerInfoTextView->SetText(NULL);
	fBurnerInfoBox->SetLabel(B_TRANSLATE_COMMENT(
		"Building in progress" B_UTF8_ELLIPSIS, "Status notification"));
	fBurnerThread = new CommandThread(NULL,
		new BInvoker(new BMessage(kBurnerMessage), this));

	AppSettings* settings = my_app->Settings();
	if (settings->Lock()) {
		settings->GetCacheFolder(*fImagePath);
		settings->Unlock();
	}
	if (fImagePath->InitCheck() != B_OK)
		return;

	fNotification.SetGroup("BurnItNow");
	fNotification.SetMessageID("BurnItNow_Data");
	fNotification.SetTitle(B_TRANSLATE("Building data image"));
	fNotification.SetContent(B_TRANSLATE("Preparing the build" B_UTF8_ELLIPSIS));
	fNotification.SetProgress(0);
	 // It may take a while for the building to start...
	fNotification.Send(60 * 1000000LL);

	BString discLabel;
	if (fDiscLabel->TextView()->TextLength() == 0)
		discLabel = fDirPath->Leaf();
	else
		discLabel = fDiscLabel->Text();

	status_t ret = fImagePath->Append(kCacheFileData);
	if (ret == B_OK) {
		step = BUILDING;	// flag we're building ISO

		fBurnerThread->AddArgument("mkisofs")
			->AddArgument("-iso-level 3")
			->AddArgument("-J")
			->AddArgument("-joliet-long")
			->AddArgument("-rock")
			->AddArgument("-V")
			->AddArgument(discLabel)
			->AddArgument("-o")
			->AddArgument(fImagePath->Path())
			->AddArgument(fDirPath->Path())
			->Run();
	}
}


void
CompilationDataView::BurnDisc()
{
	if (fImagePath->Path() == NULL) {
		(new BAlert("ChooseDirectoryFirstAlert", B_TRANSLATE(
			"First build an image to burn."), B_TRANSLATE("OK")))->Go();
		return;
	}
	if (fImagePath->InitCheck() != B_OK)
		return;

	if (fBurnerThread != NULL)
		delete fBurnerThread;

	step = BURNING;	// flag we're burning

	fBurnerInfoTextView->SetText(NULL);
	fBurnerInfoBox->SetLabel(B_TRANSLATE_COMMENT(
		"Burning in progress" B_UTF8_ELLIPSIS,"Status notification"));
	fChooseButton->SetEnabled(false);
	fImageButton->SetEnabled(false);
	fBurnButton->SetEnabled(false);

	fNotification.SetGroup("BurnItNow");
	fNotification.SetMessageID("BurnItNow_Data");
	fNotification.SetTitle(B_TRANSLATE("Burning data disc"));
	fNotification.SetProgress(0);
	fNotification.Send(60 * 1000000LL);

	BString device("dev=");
	device.Append(windowParent->GetSelectedDevice().number.String());
	sessionConfig config = windowParent->GetSessionConfig();

	fBurnerThread = new CommandThread(NULL,
		new BInvoker(new BMessage(kBurnerMessage), this));
	fBurnerThread->AddArgument("cdrecord");

	if (config.simulation)
		fBurnerThread->AddArgument("-dummy");
	if (config.eject)
		fBurnerThread->AddArgument("-eject");
	if (config.speed != "")
		fBurnerThread->AddArgument(config.speed);

	fBurnerThread->AddArgument(config.mode)
		->AddArgument("fs=16m")
		->AddArgument(device)
		->AddArgument("-v")	// to get progress output
		->AddArgument("-gracetime=2")
		->AddArgument("-pad")
		->AddArgument("padsize=63s")
		->AddArgument(fImagePath->Path())
		->Run();

	fParser.Reset();
}


int32
CompilationDataView::InProgress()
{
	printf("step: %i\n", step);
	return step;
}

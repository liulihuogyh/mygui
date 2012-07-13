/*!
	@file
	@author		Albert Semenov
	@date		08/2010
*/
#include "Precompiled.h"
#include "EditorState.h"
#include "Application.h"
#include "FileSystemInfo/FileSystemInfo.h"
#include "Localise.h"
#include "CommandManager.h"
#include "ActionManager.h"
#include "MessageBoxManager.h"
#include "Tools/DialogManager.h"
#include "StateManager.h"
#include "RecentFilesManager.h"
#include "ExportManager.h"

namespace tools
{
	EditorState::EditorState() :
		mFileName("unnamed.xml"),
		mDefaultFileName("unnamed.xml"),
		mMainPane(nullptr),
		mOpenSaveFileDialog(nullptr),
		mMessageBoxFadeControl(nullptr),
		mSettingsWindow(nullptr)
	{
		CommandManager::getInstance().registerCommand("Command_FileDrop", MyGUI::newDelegate(this, &EditorState::commandFileDrop));
		CommandManager::getInstance().registerCommand("Command_FileLoad", MyGUI::newDelegate(this, &EditorState::commandLoad));
		CommandManager::getInstance().registerCommand("Command_FileSave", MyGUI::newDelegate(this, &EditorState::commandSave));
		CommandManager::getInstance().registerCommand("Command_FileSaveAs", MyGUI::newDelegate(this, &EditorState::commandSaveAs));
		CommandManager::getInstance().registerCommand("Command_ClearAll", MyGUI::newDelegate(this, &EditorState::commandClear));
		CommandManager::getInstance().registerCommand("Command_Settings", MyGUI::newDelegate(this, &EditorState::commandSettings));
		//CommandManager::getInstance().registerCommand("Command_Test", MyGUI::newDelegate(this, &EditorState::commandTest));
		CommandManager::getInstance().registerCommand("Command_RecentFiles", MyGUI::newDelegate(this, &EditorState::commandRecentFiles));
		CommandManager::getInstance().registerCommand("Command_Quit", MyGUI::newDelegate(this, &EditorState::commandQuit));
		CommandManager::getInstance().registerCommand("Command_Undo", MyGUI::newDelegate(this, &EditorState::commandUndo));
		CommandManager::getInstance().registerCommand("Command_Redo", MyGUI::newDelegate(this, &EditorState::commandRedo));
	}

	EditorState::~EditorState()
	{
	}

	void EditorState::initState()
	{
		addUserTag("\\n", "\n");
		addUserTag("CurrentFileName", mFileName);

		mMainPane = new Control();
		mMainPane->Initialise("MainPane.layout");

		mMessageBoxFadeControl = new MessageBoxFadeControl();

		mSettingsWindow = new SettingsWindow();
		mSettingsWindow->eventEndDialog = MyGUI::newDelegate(this, &EditorState::notifySettingsWindowEndDialog);

		mOpenSaveFileDialog = new OpenSaveFileDialog();
		mOpenSaveFileDialog->eventEndDialog = MyGUI::newDelegate(this, &EditorState::notifyEndDialog);
		mOpenSaveFileDialog->setFileMask("*.xml");
		mOpenSaveFileDialog->setCurrentFolder(RecentFilesManager::getInstance().getRecentFolder());
		mOpenSaveFileDialog->setRecentFolders(RecentFilesManager::getInstance().getRecentFolders());

		ActionManager::getInstance().eventChanges.connect(this, &EditorState::notifyChanges);

		updateCaption();

		if (!Application::getInstance().getParams().empty())
		{
			mFileName = Application::getInstance().getParams().front();
			addUserTag("CurrentFileName", mFileName);

			load();
			updateCaption();
		}
	}

	void EditorState::cleanupState()
	{
		ActionManager::getInstance().eventChanges.disconnect(this);

		mOpenSaveFileDialog->eventEndDialog = nullptr;
		delete mOpenSaveFileDialog;
		mOpenSaveFileDialog = nullptr;

		delete mSettingsWindow;
		mSettingsWindow = nullptr;

		delete mMessageBoxFadeControl;
		mMessageBoxFadeControl = nullptr;

		delete mMainPane;
		mMainPane = nullptr;
	}

	void EditorState::pauseState()
	{
		mMainPane->getRoot()->setVisible(false);
	}

	void EditorState::resumeState()
	{
		mMainPane->getRoot()->setVisible(true);
	}

	void EditorState::updateCaption()
	{
		addUserTag("HasChanged", ActionManager::getInstance().getChanges() ? "*" : "");

		CommandManager::getInstance().executeCommand("Command_UpdateAppCaption");
	}

	void EditorState::commandLoad(const MyGUI::UString& _commandName, bool& _result)
	{
		if (!checkCommand())
			return;

		if (ActionManager::getInstance().getChanges())
		{
			MyGUI::Message* message = MessageBoxManager::getInstance().create(
				replaceTags("Warning"),
				replaceTags("MessageUnsavedData"),
				MyGUI::MessageBoxStyle::IconQuest
					| MyGUI::MessageBoxStyle::Yes
					| MyGUI::MessageBoxStyle::No
					| MyGUI::MessageBoxStyle::Cancel);
			message->eventMessageBoxResult += MyGUI::newDelegate(this, &EditorState::notifyMessageBoxResultLoad);
		}
		else
		{
			showLoadWindow();
		}

		_result = true;
	}

	void EditorState::commandSave(const MyGUI::UString& _commandName, bool& _result)
	{
		if (!checkCommand())
			return;

		if (ActionManager::getInstance().getChanges())
		{
			save();
		}

		_result = true;
	}

	void EditorState::commandSaveAs(const MyGUI::UString& _commandName, bool& _result)
	{
		if (!checkCommand())
			return;

		showSaveAsWindow();

		_result = true;
	}

	void EditorState::commandClear(const MyGUI::UString& _commandName, bool& _result)
	{
		if (!checkCommand())
			return;

		if (ActionManager::getInstance().getChanges())
		{
			MyGUI::Message* message = MessageBoxManager::getInstance().create(
				replaceTags("Warning"),
				replaceTags("MessageUnsavedData"),
				MyGUI::MessageBoxStyle::IconQuest
					| MyGUI::MessageBoxStyle::Yes
					| MyGUI::MessageBoxStyle::No
					| MyGUI::MessageBoxStyle::Cancel);
			message->eventMessageBoxResult += MyGUI::newDelegate(this, &EditorState::notifyMessageBoxResultClear);
		}
		else
		{
			clear();
		}

		_result = true;
	}

	void EditorState::commandQuit(const MyGUI::UString& _commandName, bool& _result)
	{
		if (!checkCommand())
			return;

		if (ActionManager::getInstance().getChanges())
		{
			MyGUI::Message* message = MessageBoxManager::getInstance().create(
				replaceTags("Warning"),
				replaceTags("MessageUnsavedData"),
				MyGUI::MessageBoxStyle::IconQuest
					| MyGUI::MessageBoxStyle::Yes
					| MyGUI::MessageBoxStyle::No
					| MyGUI::MessageBoxStyle::Cancel);
			message->eventMessageBoxResult += MyGUI::newDelegate(this, &EditorState::notifyMessageBoxResultQuit);
		}
		else
		{
			StateManager::getInstance().stateEvent(this, "Exit");
		}

		_result = true;
	}

	void EditorState::commandFileDrop(const MyGUI::UString& _commandName, bool& _result)
	{
		if (!checkCommand())
			return;

		mDropFileName = CommandManager::getInstance().getCommandData();
		if (mDropFileName.empty())
			return;

		if (ActionManager::getInstance().getChanges())
		{
			MyGUI::Message* message = MessageBoxManager::getInstance().create(
				replaceTags("Warning"),
				replaceTags("MessageUnsavedData"),
				MyGUI::MessageBoxStyle::IconQuest
					| MyGUI::MessageBoxStyle::Yes
					| MyGUI::MessageBoxStyle::No
					| MyGUI::MessageBoxStyle::Cancel);
			message->eventMessageBoxResult += MyGUI::newDelegate(this, &EditorState::notifyMessageBoxResultLoadDropFile);
		}
		else
		{
			clear();

			loadDropFile();
		}

		_result = true;
	}

	/*void EditorState::commandTest(const MyGUI::UString& _commandName, bool& _result)
	{
		if (!checkCommand())
			return;

		SkinItem* item = SkinManager::getInstance().getItemSelected();
		if (item != nullptr)
			StateManager::getInstance().stateEvent(this, "Test");

		_result = true;
	}*/

	void EditorState::notifyMessageBoxResultLoad(MyGUI::Message* _sender, MyGUI::MessageBoxStyle _result)
	{
		if (_result == MyGUI::MessageBoxStyle::Yes)
		{
			save();
			clear();

			showLoadWindow();
		}
		else if (_result == MyGUI::MessageBoxStyle::No)
		{
			clear();

			showLoadWindow();
		}
	}

	void EditorState::notifyMessageBoxResultLoadDropFile(MyGUI::Message* _sender, MyGUI::MessageBoxStyle _result)
	{
		if (_result == MyGUI::MessageBoxStyle::Yes)
		{
			save();
			clear();

			loadDropFile();
		}
		else if (_result == MyGUI::MessageBoxStyle::No)
		{
			clear();

			loadDropFile();
		}
	}

	void EditorState::loadDropFile()
	{
		mFileName = mDropFileName;
		addUserTag("CurrentFileName", mFileName);

		load();
		updateCaption();
	}

	void EditorState::showLoadWindow()
	{
		mOpenSaveFileDialog->setCurrentFolder(RecentFilesManager::getInstance().getRecentFolder());
		mOpenSaveFileDialog->setRecentFolders(RecentFilesManager::getInstance().getRecentFolders());
		mOpenSaveFileDialog->setDialogInfo(replaceTags("CaptionOpenFile"), replaceTags("ButtonOpenFile"));
		mOpenSaveFileDialog->setMode("Load");
		mOpenSaveFileDialog->doModal();
	}

	void EditorState::notifyEndDialog(Dialog* _sender, bool _result)
	{
		if (_result)
		{
			if (mOpenSaveFileDialog->getMode() == "SaveAs")
			{
				RecentFilesManager::getInstance().setRecentFolder(mOpenSaveFileDialog->getCurrentFolder());
				mFileName = common::concatenatePath(mOpenSaveFileDialog->getCurrentFolder(), mOpenSaveFileDialog->getFileName());
				addUserTag("CurrentFileName", mFileName);

				save();
				updateCaption();
			}
			else if (mOpenSaveFileDialog->getMode() == "Load")
			{
				RecentFilesManager::getInstance().setRecentFolder(mOpenSaveFileDialog->getCurrentFolder());
				mFileName = common::concatenatePath(mOpenSaveFileDialog->getCurrentFolder(), mOpenSaveFileDialog->getFileName());
				addUserTag("CurrentFileName", mFileName);

				load();
				updateCaption();
			}
		}

		mOpenSaveFileDialog->endModal();
	}

	void EditorState::notifyMessageBoxResultClear(MyGUI::Message* _sender, MyGUI::MessageBoxStyle _result)
	{
		if (_result == MyGUI::MessageBoxStyle::Yes)
		{
			save();
			clear();
		}
		else if (_result == MyGUI::MessageBoxStyle::No)
		{
			clear();
		}
	}

	void EditorState::showSaveAsWindow()
	{
		mOpenSaveFileDialog->setCurrentFolder(RecentFilesManager::getInstance().getRecentFolder());
		mOpenSaveFileDialog->setRecentFolders(RecentFilesManager::getInstance().getRecentFolders());
		mOpenSaveFileDialog->setDialogInfo(replaceTags("CaptionSaveFile"), replaceTags("ButtonSaveFile"));
		mOpenSaveFileDialog->setMode("SaveAs");
		mOpenSaveFileDialog->doModal();
	}

	void EditorState::notifyMessageBoxResultQuit(MyGUI::Message* _sender, MyGUI::MessageBoxStyle _result)
	{
		if (_result == MyGUI::MessageBoxStyle::Yes)
		{
			save();
			StateManager::getInstance().stateEvent(this, "Exit");
		}
		else if (_result == MyGUI::MessageBoxStyle::No)
		{
			StateManager::getInstance().stateEvent(this, "Exit");
		}
	}

	void EditorState::notifyChanges()
	{
		updateCaption();
	}

	bool EditorState::checkCommand()
	{
		if (DialogManager::getInstance().getAnyDialog())
			return false;

		if (MessageBoxManager::getInstance().hasAny())
			return false;

		if (!StateManager::getInstance().getStateActivate(this))
			return false;

		return true;
	}

	void EditorState::clear()
	{
		// export manager reset
		//SkinManager::getInstance().clear();

		ActionManager::getInstance().reset();

		mFileName = mDefaultFileName;
		addUserTag("CurrentFileName", mFileName);

		updateCaption();
	}

	void EditorState::load()
	{
		// export manager reset
		//SkinManager::getInstance().clear();

		ActionManager::getInstance().reset();

		MyGUI::xml::Document doc;
		if (doc.open(mFileName))
		{
			bool result = ExportManager::getInstance().deserialization(doc);
			if (result)
			{
				if (mFileName != mDefaultFileName)
					RecentFilesManager::getInstance().addRecentFile(mFileName);
			}
			else
			{
				/*MyGUI::Message* message =*/ MessageBoxManager::getInstance().create(
					replaceTags("Error"),
					replaceTags("MessageIncorrectFileFormat"),
					MyGUI::MessageBoxStyle::IconError
						| MyGUI::MessageBoxStyle::Yes);

				mFileName = mDefaultFileName;
				addUserTag("CurrentFileName", mFileName);

				updateCaption();
			}
		}
		else
		{
			/*MyGUI::Message* message =*/ MessageBoxManager::getInstance().create(
				replaceTags("Error"),
				doc.getLastError(),
				MyGUI::MessageBoxStyle::IconError
					| MyGUI::MessageBoxStyle::Yes);
		}
	}

	bool EditorState::save()
	{
		MyGUI::xml::Document doc;
		doc.createDeclaration();

		ExportManager::getInstance().serialization(doc);

		bool result = doc.save(mFileName);

		if (result)
		{
			if (mFileName != mDefaultFileName)
				RecentFilesManager::getInstance().addRecentFile(mFileName);

			ActionManager::getInstance().saveChanges();
			return true;
		}

		/*MyGUI::Message* message =*/ MessageBoxManager::getInstance().create(
			replaceTags("Error"),
			doc.getLastError(),
			MyGUI::MessageBoxStyle::IconError
				| MyGUI::MessageBoxStyle::Yes);

		return false;
	}

	void EditorState::commandRecentFiles(const MyGUI::UString& _commandName, bool& _result)
	{
		commandFileDrop(_commandName, _result);
	}

	void EditorState::commandSettings(const MyGUI::UString& _commandName, bool& _result)
	{
		mSettingsWindow->doModal();

		_result = true;
	}

	void EditorState::notifySettingsWindowEndDialog(Dialog* _dialog, bool _result)
	{
		MYGUI_ASSERT(mSettingsWindow == _dialog, "mSettingsWindow == _sender");

		if (_result)
			mSettingsWindow->saveSettings();

		mSettingsWindow->endModal();
	}

	void EditorState::commandUndo(const MyGUI::UString& _commandName, bool& _result)
	{
		if (!checkCommand())
			return;

		ActionManager::getInstance().undoAction();

		_result = true;
	}

	void EditorState::commandRedo(const MyGUI::UString& _commandName, bool& _result)
	{
		if (!checkCommand())
			return;

		ActionManager::getInstance().redoAction();

		_result = true;
	}
}
#pragma once
// banana-ui (kiwami): the menu's library, read by Settings' RetroAchievements pages for the games on this card (their
// titles as the menu shows them, their files, the progress the emulators recorded). main.cpp sets it once the library
// exists (its kiwami: line); it is read only under the host's uiLock, as Settings is, and never kept beyond a call.
namespace banana
{
	class Library;
	namespace kiwami { extern Library* library; }
}

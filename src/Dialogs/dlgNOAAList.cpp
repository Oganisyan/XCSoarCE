/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2011 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Dialogs/Weather.hpp"
#include "Dialogs/Dialogs.h"
#include "Dialogs/Message.hpp"
#include "Dialogs/JobDialog.hpp"
#include "Language/Language.hpp"
#include "Net/Features.hpp"

#ifdef HAVE_NET

#include "Dialogs/TextEntry.hpp"
#include "Dialogs/XML.hpp"
#include "Dialogs/dlgTools.h"
#include "Form/List.hpp"
#include "Form/Button.hpp"
#include "Screen/Layout.hpp"
#include "Screen/Fonts.hpp"
#include "Weather/NOAAGlue.hpp"
#include "Weather/NOAAStore.hpp"
#include "Weather/METAR.hpp"
#include "Util/TrivialArray.hpp"
#include "MainWindow.hpp"
#include "Compiler.h"

#include <stdio.h>

static WndForm *wf;
static WndListFrame *station_list;

struct NOAAListItem
{
  StaticString<5> code;
  NOAAStore::iterator iterator;

  bool operator<(const NOAAListItem &i2) const {
    return _tcscmp(code, i2.code) == -1;
  }
};

static TrivialArray<NOAAListItem, 20> list;

static void
UpdateList()
{
  list.clear();

  for (auto i = noaa_store->begin(), end = noaa_store->end(); i != end; ++i) {
    NOAAListItem item;
    item.code = i->GetCodeT();
    item.iterator = i;
    list.push_back(item);
  }

  std::sort(list.begin(), list.end());

  station_list->SetLength(list.size());
  station_list->invalidate();
}

static void
PaintListItem(Canvas &canvas, const PixelRect rc, unsigned index)
{
  assert(index < list.size());

  const Font &code_font = Fonts::MapBold;
  const Font &details_font = Fonts::MapLabel;

  canvas.select(code_font);

  canvas.text_clipped(rc.left + Layout::FastScale(2),
                      rc.top + Layout::FastScale(2), rc, list[index].code);

  canvas.select(details_font);

  METAR metar;
  const TCHAR *tmp;
  if (!list[index].iterator->GetMETAR(metar))
    tmp = _("No METAR available");
  else
    tmp = metar.content.c_str();

  canvas.text_clipped(rc.left + Layout::FastScale(2),
                      rc.top + code_font.GetHeight() + Layout::FastScale(4),
                      rc, tmp);
}

static void
AddClicked(gcc_unused WndButton &button)
{
  TCHAR code[5] = _T("");
  if (!dlgTextEntryShowModal(*(SingleWindow *)button.get_root_owner(),
                             code, 5, _("Airport ICAO code")))
    return;

  if (_tcslen(code) != 4) {
    MessageBoxX(_("Please enter the FOUR letter code of the desired station."),
                _("Error"), MB_OK);
    return;
  }

  for (unsigned i = 0; i < 4; i++) {
    if (code[i] < _T('A') || code[i] > _T('Z')) {
      MessageBoxX(_("Please don't use special characters in the four letter code of the desired station."),
                  _("Error"), MB_OK);
      return;
    }
  }

  NOAAStore::iterator i = noaa_store->AddStation(code);
  noaa_store->SaveToProfile();

  DialogJobRunner runner(wf->GetMainWindow(), wf->GetLook(),
                         _("Download"), true);
  i->Update(runner);

  UpdateList();
}

static void
UpdateClicked(gcc_unused WndButton &Sender)
{
  DialogJobRunner runner(wf->GetMainWindow(), wf->GetLook(),
                         _("Download"), true);
  noaa_store->Update(runner);
  UpdateList();
}

static void
RemoveClicked(gcc_unused WndButton &Sender)
{
  unsigned index = station_list->GetCursorIndex();
  assert(index < list.size());

  TCHAR tmp[256];
  _stprintf(tmp, _("Do you want to remove station %s?"),
            list[index].code.c_str());

  if (MessageBoxX(tmp, _("Remove"), MB_YESNO) == IDNO)
    return;

  noaa_store->erase(list[index].iterator);
  noaa_store->SaveToProfile();

  UpdateList();
}

static void
CloseClicked(gcc_unused WndButton &Sender)
{
  wf->SetModalResult(mrOK);
}

static void
OpenDetails(unsigned index)
{
  assert(index < list.size());
  dlgNOAADetailsShowModal(*(SingleWindow *)wf->get_root_owner(),
                          list[index].iterator);
  UpdateList();
}

static void
DetailsClicked(gcc_unused WndButton &Sender)
{
  if (station_list->GetLength() > 0)
    OpenDetails(station_list->GetCursorIndex());
}

static void
ListItemSelected(unsigned index)
{
  OpenDetails(index);
}

static gcc_constexpr_data CallBackTableEntry CallBackTable[] = {
  DeclareCallBackEntry(PaintListItem),
  DeclareCallBackEntry(ListItemSelected),
  DeclareCallBackEntry(AddClicked),
  DeclareCallBackEntry(UpdateClicked),
  DeclareCallBackEntry(RemoveClicked),
  DeclareCallBackEntry(CloseClicked),
  DeclareCallBackEntry(DetailsClicked),
  DeclareCallBackEntry(NULL)
};

void
dlgNOAAListShowModal(SingleWindow &parent)
{
  wf = LoadDialog(CallBackTable, parent, Layout::landscape ?
                  _T("IDR_XML_NOAA_LIST_L") :
                  _T("IDR_XML_NOAA_LIST"));
  assert(wf != NULL);

  UPixelScalar item_height = Fonts::MapBold.GetHeight() + Layout::Scale(6) +
                         Fonts::MapLabel.GetHeight();

  station_list = (WndListFrame *)wf->FindByName(_T("StationList"));
  assert(station_list != NULL);
  station_list->SetItemHeight(item_height);
  station_list->SetPaintItemCallback(PaintListItem);
  station_list->SetActivateCallback(ListItemSelected);
  UpdateList();

  wf->ShowModal();

  delete wf;
}

#else
void
dlgNOAAListShowModal(SingleWindow &parent)
{
  MessageBoxX(_("This function is not available on your platform yet."),
              _("Error"), MB_OK);
}
#endif

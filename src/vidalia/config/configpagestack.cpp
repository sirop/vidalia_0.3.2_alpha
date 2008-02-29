/*
**  This file is part of Vidalia, and is subject to the license terms in the
**  LICENSE file, found in the top level directory of this distribution. If you
**  did not receive the LICENSE file with this file, you may obtain it from the
**  Vidalia source package distributed by the Vidalia Project at
**  http://www.vidalia-project.net/. No part of Vidalia, including this file,
**  may be copied, modified, propagated, or distributed except according to the
**  terms described in the LICENSE file.
*/

/*
** \file configpagestack.cpp
** \version $Id$
** \brief A collection of configuration pages
*/

#include <QAction>
#include "configpagestack.h"


/** Default constructor. */
ConfigPageStack::ConfigPageStack(QWidget *parent)
: QStackedWidget(parent)
{
}

/** Adds a page to the stack. */
void
ConfigPageStack::add(ConfigPage *page, QAction *action)
{
  _pages.insert(action, page);
  insertWidget(count(), page);
}

/** Sets the current config page and checks its action. */
void
ConfigPageStack::setCurrentPage(ConfigPage *page)
{
  foreach (QAction *action, _pages.keys(page)) {
    action->setChecked(true);
  }
  setCurrentWidget(page);
}

/** Sets the current config page index and checks its action. */
void
ConfigPageStack::setCurrentIndex(int index)
{
  setCurrentPage((ConfigPage *)widget(index));
}

/** Shows the config page associated with the activated action. */
void
ConfigPageStack::showPage(QAction *pageAction)
{
  setCurrentWidget(_pages.value(pageAction));
}


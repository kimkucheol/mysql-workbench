/* 
 * Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "grtdb/db_object_helpers.h"

#include "grts/structs.h"
#include "grts/structs.db.mgmt.h"
#include "grts/structs.db.mysql.h"

#include "grtpp.h"
#include "grt/grt_reporter.h"

using namespace grt;

#include "grti/wbvalidation.h"

#include "db_mysql_validation_page.h"
#include "diff_tree.h"
#include "db_mysql_sql_export.h"


DbMySQLValidationPage::DbMySQLValidationPage(bec::GRTManager *grtm)
  : _manager(grtm)
{
  messages_list = grtm->get_messages_list()->create_list();
}

DbMySQLValidationPage::~DbMySQLValidationPage()
{
  delete messages_list;
}

//--------------------------------------------------------------------------------------------------

void DbMySQLValidationPage::run_validation()
{
  bec::GRTTask::Ref task = bec::GRTTask::create_task("Catalog validation", 
    _manager->get_dispatcher(), 
    boost::bind(&DbMySQLSQLExport::validation_task, this, _1, grt::StringRef()));

  scoped_connect(task->signal_message(),boost::bind(&DbMySQLSQLExport::validation_message, this, _1));
  scoped_connect(task->signal_finished(),boost::bind(&DbMySQLSQLExport::validation_finished, this, _1));
  _manager->get_dispatcher()->add_task(task);
}

//--------------------------------------------------------------------------------------------------

void DbMySQLValidationPage::validation_finished(grt::ValueRef res)
{
  if (_validation_finished_cb)
    _validation_finished_cb();
}


void DbMySQLValidationPage::validation_message(const grt::Message &msg)
{
  switch (msg.type)
  {
  case grt::ErrorMsg:
  case grt::WarningMsg:
  case grt::InfoMsg:
  case grt::OutputMsg:
    _manager->get_messages_list()->handle_message(msg);
    break;
    
  case grt::ProgressMsg:
    // XXX implement ProgressMsg
    break;
  default:
    break;
  }
}


ValueRef DbMySQLValidationPage::validation_task(grt::GRT* grt, grt::StringRef)
{
  try
  {
    std::vector<WbValidationInterfaceWrapper*> validation_modules= grt->get_implementing_modules<WbValidationInterfaceWrapper>();
  
    if (validation_modules.empty())
      return grt::StringRef("\nSQL Script Export Error: Not able to locate 'Validation' modules");


    GrtObjectRef catalog(GrtObjectRef::cast_from(_manager->get_grt()->get("/wb/doc/physicalModels/0/catalog")));

    for (std::vector<WbValidationInterfaceWrapper*>::iterator module= validation_modules.begin();
         module != validation_modules.end(); ++module)
    {
      std::string caption= (*module)->getValidationDescription(catalog);
      
      if (!caption.empty())
      {
        grt->send_info("Starting "+caption);

        int validation_res= (int)(*module)->validate("All", catalog);

        _manager->get_dispatcher()->call_from_main_thread<int>(
			boost::bind(_validation_step_finished_cb, validation_res), true, false);
      }
    }
  }
  catch(std::exception& ex)
  {
    if (ex.what())
      return grt::StringRef(std::string("\nCatalog Validation Error: ").append(ex.what()).c_str());
    else
      return grt::StringRef("\nUnknown Catalog Validation Error");
  }

  return grt::StringRef("");
}

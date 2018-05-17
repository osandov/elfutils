/* Find the split (or skeleton) unit for a given unit.
   Copyright (C) 2018 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libdwP.h"
#include "libelfP.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


Dwarf_CU *
internal_function
__libdw_find_split_unit (Dwarf_CU *cu)
{
  /* Only try once.  */
  if (cu->split != (Dwarf_CU *) -1)
    return cu->split;

  /* We need a skeleton unit with a comp_dir and [GNU_]dwo_name attributes.
     The split unit will be the first in the dwo file and should have the
     same id as the skeleton.  */
  if (cu->unit_type == DW_UT_skeleton)
    {
      Dwarf_Die cudie = CUDIE (cu);
      Dwarf_Attribute compdir, dwo_name;
      /* It is fine if compdir doesn't exists, but then dwo_name needs
	 to be an absolute path.  Also try relative path first.  */
      dwarf_attr (&cudie, DW_AT_comp_dir, &compdir);
	if (dwarf_attr (&cudie, DW_AT_dwo_name, &dwo_name) != NULL
	    || dwarf_attr (&cudie, DW_AT_GNU_dwo_name, &dwo_name) != NULL)
	{
	  const char *comp_dir = dwarf_formstring (&compdir);
	  const char *dwo_file = dwarf_formstring (&dwo_name);
	  const char *debugdir = cu->dbg->debugdir;
	  char *dwo_path = __libdw_filepath (debugdir, NULL, dwo_file);
	  if (dwo_path == NULL && comp_dir != NULL)
	    dwo_path = __libdw_filepath (debugdir, comp_dir, dwo_file);
	  if (dwo_path != NULL)
	    {
	      int split_fd = open (dwo_path, O_RDONLY);
	      if (split_fd != -1)
		{
		  Dwarf *split_dwarf = dwarf_begin (split_fd, DWARF_C_READ);
		  if (split_dwarf != NULL)
		    {
		      Dwarf_CU *split = NULL;
		      while (dwarf_get_units (split_dwarf, split, &split,
					      NULL, NULL, NULL, NULL) == 0)
			{
			  if (split->unit_type == DW_UT_split_compile
			      && cu->unit_id8 == split->unit_id8)
			    {
			      /* Link skeleton and split compule units.  */
			      cu->split = split;
			      split->split = cu;

			      /* We have everything we need from this
				 ELF file.  And we are going to close
				 the fd to not run out of file
				 descriptors.  */
			      elf_cntl (split_dwarf->elf, ELF_C_FDDONE);
			      break;
			    }

			  if (cu->split == (Dwarf_CU *) -1)
			    dwarf_end (split_dwarf);
			}
		      /* Always close, because we don't want to run
			 out of file descriptors.  See also the
			 elf_fcntl ELF_C_FDDONE call above.  */
		    }
		  close (split_fd);
		}
	      free (dwo_path);
	    }
	}
    }

  /* If we found nothing, make sure we don't try again.  */
  if (cu->split == (Dwarf_CU *) -1)
    cu->split = NULL;

  return cu->split;
}
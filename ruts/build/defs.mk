##
#
#  Multi Process Garbage Collector
#  Copyright © 2016 Hewlett Packard Enterprise Development Company LP.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#  As an exception, the copyright holders of this Library grant you permission
#  to (i) compile an Application with the Library, and (ii) distribute the 
#  Application containing code generated by the Library and added to the 
#  Application during this compilation process under terms of your choice, 
#  provided you also meet the terms and conditions of the Application license.
#

###########################
#
# This makefile contains definitions that the projects.mk file may
# want to take advantage of
#
###########################

this_mf := $(lastword $(MAKEFILE_LIST))

# The directory that this makefile is in, assumed to be <proj>/build

build_dir := $(abspath $(dir $(this_mf)))

# The project directory

project_dir := $(abspath $(build_dir)/..)

# The name of the project.  This will be used, e.g., for the default lib name

project_name ?= $(notdir $(project_dir))

# The build config directory, assumed to be where the overall make was
# run from.

config := $(notdir $(CURDIR))

# The git base dir will be the first ancestor of the build dir that
# doesn't itself have a build dir.  This allows projects to be nested
# within one another.  This function does the necessary recursion

ancestor_without_build = $(if $(wildcard $1/../build),$(call ancestor_without_build,$1/..),$(abspath $1/..))

# The dir above this project and any project it's nested in.

git_base_dir := $(call ancestor_without_build,$(project_dir))



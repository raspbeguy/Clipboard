/*  Clipboard - Cut, copy, and paste anything, anywhere, all from the terminal.
    Copyright (C) 2023 Jackson Huff and other contributors on GitHub.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.*/
#include "xdg_toplevel.hpp"
#include "all.hpp"

XdgToplevel::XdgToplevel(XdgSurface const& surface)
    : WlObject { xdg_surface_get_toplevel(surface.value()) }
{ }

void XdgToplevel::setTitle(char const* title) const {
    xdg_toplevel_set_title(value(), title);
}

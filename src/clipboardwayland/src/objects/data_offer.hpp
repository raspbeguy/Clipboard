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
#pragma once

#include "spec.hpp"
#include "forward.hpp"

#include <string>
#include <set>

struct WlDataOfferSpec {
    WL_SPEC_BASE(wl_data_offer, 3)
    WL_SPEC_DESTROY(wl_data_offer)
    WL_SPEC_LISTENER(wl_data_offer)
};

class WlDataOffer : public WlObject<WlDataOfferSpec> {
    friend WlDataOfferSpec;

    std::set<std::string> m_mimeTypes {};

public:
    explicit WlDataOffer(obj_t* value) : WlObject<spec_t> { value } { }

    void receive(std::string_view mime, int fd) const;

    /** Performs an action for each MIME Type supported by this offer. */
    template<std::invocable<std::string const&> func_t>
    void forEachMimeType(func_t func) const;

private:
    void onOffer(char const*);
};

template<std::invocable<std::string const&> func_t>
void WlDataOffer::forEachMimeType(func_t func) const {
    for (auto&& value : m_mimeTypes) {
        func(value);
    }
}
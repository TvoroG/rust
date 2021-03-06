// Copyright 2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

fn main() {
    let x = 1i;
    proc() { x = 2; };
    //~^ ERROR: cannot assign to immutable captured outer variable in a proc `x`

    let s = std::io::stdin();
    proc() { s.read_to_end(); };
    //~^ ERROR: cannot borrow immutable captured outer variable in a proc `s` as mutable
}

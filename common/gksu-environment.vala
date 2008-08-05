/*
 * Copyright (C) 2008 Gustavo Noronha Silva
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.  You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

using GLib;
using Gee;

namespace Gksu {
	class Variable: Object {
		public string name;
		public string regex;
	}

	public class Environment: Object {
		HashMap<string,Variable> variables;

		construct {
			weak string[] search_path = GLib.Environment.get_system_data_dirs();
			variables = new HashMap<string,Variable>();

			foreach(string path in search_path) {
				string full_path = path.concat("gksu-polkit-1/environment/");
				read_variables_from_path(full_path);
			}
		}

		public HashTable<string,string>? get_variables() {
			Set<string> keysset = variables.get_keys();
			HashTable<string,string> envpairs = new HashTable<string,string>(str_hash, str_equal);

			foreach(string variable in keysset) {
				string value = GLib.Environment.get_variable(variable);
				envpairs.insert(variable, value);
			}

			return envpairs;
		}

		private void read_variables_from_path(string path) {
			Dir directory;

			try {
				directory = Dir.open(path, 0);
			} catch (FileError error) {
				return;
			}

			weak string entry;
			while((entry = directory.read_name()) != null) {
				if(entry.has_suffix(".variables")) {
					string full_path = path.concat(entry);
					read_variables_from_file(full_path);
				}
			}
		}

		private void read_variables_from_file(string path) {
			KeyFile file = new KeyFile();
			string[] variable_names;

			try {
				file.load_from_file(path, 0);
			} catch (FileError error) {
				warning("%s", error.message);
				return;
			}

			variable_names = file.get_groups();
			foreach(string name in variable_names) {
				string policy;
				try {
					policy = file.get_value(name, "Policy");
				} catch (KeyFileError error) {}

				if(policy != "send")
					continue;

				Variable variable = new Variable();
				variable.name = name;
				try {
					variable.regex = file.get_value(name, "Regex");
				} catch (KeyFileError error) {}

				variables.set(name, variable);
			}
		}
	}
}
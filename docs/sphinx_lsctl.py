# -*- coding: utf-8 -*-
from sphinx.domains import Domain, ObjType, Index
from sphinx.directives import ObjectDescription
from sphinx.roles import XRefRole
from sphinx.util.docfields import Field, GroupedField, TypedField
from sphinx import addnodes
from docutils import nodes
from sphinx.util.nodes import make_refnode
import re

class Cmd(ObjectDescription):
    doc_field_types = [
        GroupedField('scope', label=('Scopes'), names=('scope',),
               rolename='cmd'),
        TypedField('parameter', label=('Parameters'),
               names=('param', 'parameter', 'arg', 'argument'),
               typerolename='obj', typenames=('paramtype', 'type')),
    ]

    def handle_signature(self, sig, signode):
        sig = sig.split()
        name = sig.pop(0)
        cmd = [name]
        while sig[0] != '|':
            cmd.append(sig.pop(0))
        cmd = ' '.join(cmd)
        sig.pop(0)

        signode += addnodes.desc_name(cmd, cmd)
        signode += nodes.inline(' ', ' ')
        signode += nodes.Text(' '.join(sig))
        return name

    def add_target_and_index(self, name, sig, signode):
        target = 'lsctl.cmd.' + name

        if target not in self.state.document.ids:
            signode['names'].append(target)
            signode['ids'].append(target)
            signode['first'] = (not self.names)
            self.state.document.note_explicit_target(signode)

            cmds = self.env.domaindata['lsctl']['cmds']
            cmds[name] = self.env.docname

        index_name = '{} (LSCTL directive)'.format(name)
        self.indexnode['entries'].append(('single', index_name, target, '', None))

class LsctlDomain(Domain):
    name = "lsctl"
    label = "LSCTL"
    object_types = {
        "cmd": ObjType("directive")
    }
    directives = {
        "cmd": Cmd
    }
    roles = {
        "cmd": XRefRole()
    }

    initial_data = {
        'cmds': {},    # fullname -> docname, objtype
    }
    indices = [
    ]

    def resolve_xref(self, env, fromdocname, builder,
                     typ, target, node, contnode):
        if typ != 'cmd':
            return None
        obj = self.data['cmds'].get(target)
        if obj is None:
            return None

        return make_refnode(builder, fromdocname, obj, 'lsctl.cmd.'+target, contnode, target)

def setup(app):
    app.add_domain(LsctlDomain)

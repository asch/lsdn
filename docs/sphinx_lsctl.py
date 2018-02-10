# -*- coding: utf-8 -*-
from sphinx.domains import Domain, ObjType, Index
from sphinx.directives import ObjectDescription
from sphinx.roles import XRefRole
from sphinx.util.docfields import Field, GroupedField, TypedField
from sphinx.transforms import SphinxTransform
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
               typerolename='type', typenames=('paramtype', 'type')),
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
        return cmd

    def add_target_and_index(self, name, sig, signode):
        target = 'lsctl.cmd.' + name

        if target not in self.state.document.ids:
            signode['names'].append(target)
            signode['ids'].append(target)
            signode['first'] = (not self.names)
            self.state.document.note_explicit_target(signode)

            cmds = self.env.domaindata['lsctl']['cmd']
            cmds[name] = self.env.docname

        index_name = '{} (LSCTL directive)'.format(name)
        self.indexnode['entries'].append(('single', index_name, target, '', None))

class Type(ObjectDescription):
    doc_field_types = []
    def handle_signature(self, sig, signode):
        signode += addnodes.desc_name(sig, sig)
        return sig
    def add_target_and_index(self, name, sig, signode):
        target = 'lsctl.type.' + name

        if target not in self.state.document.ids:
            signode['names'].append(target)
            signode['ids'].append(target)
            signode['first'] = (not self.names)
            self.state.document.note_explicit_target(signode)

            types = self.env.domaindata['lsctl']['type']
            types[name] = self.env.docname

        index_name = '{} (LSCTL argument type)'.format(name)
        self.indexnode['entries'].append(('single', index_name, target, '', None))

class LsctlDomain(Domain):
    name = "lsctl"
    label = "LSCTL"
    object_types = {
        "cmd": ObjType("directive"),
        "type": ObjType("directive")
    }
    directives = {
        "cmd": Cmd,
        "type": Type
    }
    roles = {
        "cmd": XRefRole(),
        "type": XRefRole()
    }

    initial_data = {
        'cmd': {},
        'type': {}
    }
    indices = [
    ]

    def resolve_xref(self, env, fromdocname, builder,
                     typ, target, node, contnode):
        if typ not in ['cmd', 'type']:
            return None
        obj = self.data[typ].get(target)
        if obj is None:
            return None

        return make_refnode(builder, fromdocname, obj,
                'lsctl.{}.{}'.format(typ,target), contnode, target)

class SvgNodeTransform(SphinxTransform):
    default_priority = 500

    def apply(self):
        for node in self.document.traverse(nodes.image):
            orig_uri = node['uri']
            uri = node['uri'] = self.replaceSvg(orig_uri)
            node['candidates'] = dict(
                [(k, self.replaceSvg(v)) for (k,v) in node['candidates'].items()])
            if orig_uri != uri:
                self.env.original_image_uri[uri] = orig_uri
                self.env.images.add_file(self.env.docname, uri)

    def replaceSvg(self, text):
        if self.app.builder.name == 'latex' and text.endswith('.svg'):
            return text[:-4] + '.pdf'
        return text

def setup(app):
    app.add_domain(LsctlDomain)
    app.add_post_transform(SvgNodeTransform)
    return {'parallel_read_safe': True}

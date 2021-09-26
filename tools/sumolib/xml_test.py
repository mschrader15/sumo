# -*- coding: utf-8 -*-
# Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
# Copyright (C) 2011-2021 German Aerospace Center (DLR) and others.
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License 2.0 which is available at
# https://www.eclipse.org/legal/epl-2.0/
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the Eclipse
# Public License 2.0 are satisfied: GNU General Public License, version 2
# or later which is available at
# https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

# @file    xml.py
# @author  Michael Behrisch
# @author  Jakob Erdmann
# @date    2011-06-23

from __future__ import print_function
from __future__ import absolute_import
from contextlib import contextmanager
import functools
from multiprocessing.managers import Namespace
import os
import queue
import sys
import re
import gzip
import io
import datetime
try:
    import xml.etree.cElementTree as ET
except ImportError as e:
    print("recovering from ImportError '%s'" % e)
    import xml.etree.ElementTree as ET
from collections import namedtuple, OrderedDict
from keyword import iskeyword
from functools import reduce
import xml.sax.saxutils


# import __main__
import time
import mmap
from queue import Empty
from copy import deepcopy
import multiprocessing as mp
from types import SimpleNamespace
import uuid
# from multiprocessing import set_start_method, get_context
# set_start_method("spawn")

# import multiprocessing as mp
# mp = get_context("spawn")

# from . import version

DEFAULT_ATTR_CONVERSIONS = {
    # shape-like
    'shape': lambda coords: map(lambda xy: map(float, xy.split(',')), coords.split()),
    # float
    'speed': float,
    'length': float,
    'width': float,
    'angle': float,
    'endOffset': float,
    'radius': float,
    'contPos': float,
    'visibility': float,
    'startPos': float,
    'endPos': float,
    'position': float,
    'x': float,
    'y': float,
    'lon': float,
    'lat': float,
    'freq': float,
    # int
    'priority': int,
    'numLanes': int,
    'index': int,
    'linkIndex': int,
    'linkIndex2': int,
    'fromLane': int,
    'toLane': int,
}


def _prefix_keyword(name, warn=False):
    result = name
    # create a legal identifier (xml allows '-', ':' and '.' ...)
    result = ''.join([c for c in name if c.isalnum() or c == '_'])
    if result != name:
        if result == '':
            result == 'attr_'
        if warn:
            print("Warning: Renaming attribute '%s' to '%s' because it contains illegal characters" % (
                name, result), file=sys.stderr)
    if name == "name":
        result = 'attr_name'
        if warn:
            print("Warning: Renaming attribute '%s' to '%s' because it conflicts with a reserved field" % (
                name, result), file=sys.stderr)

    if iskeyword(name):
        result = 'attr_' + name
        if warn:
            print("Warning: Renaming attribute '%s' to '%s' because it conflicts with a python keyword" % (
                name, result), file=sys.stderr)
    return result


def compound_object(element_name, attrnames, warn=False):
    """return a class which delegates bracket access to an internal dict.
       Missing attributes are delegated to the child dict for convenience.
       @note: Care must be taken when child nodes and attributes have the same names"""
    class CompoundObject():
        _original_fields = sorted(attrnames)
        _fields = [_prefix_keyword(a, warn) for a in _original_fields]

        def __init__(self, values, child_dict=None, text=None, child_list=None):
            for name, val in zip(self._fields, values):
                self.__dict__[name] = val
            self._child_dict = child_dict if child_dict else {}
            self.name = element_name
            self._text = text
            self._child_list = child_list if child_list else []

        def getAttributes(self):
            return [(k, getattr(self, k)) for k in self._fields]

        def hasAttribute(self, name):
            return name in self._fields

        def getAttribute(self, name):
            if self.hasAttribute(name):
                return self.__dict__[name]
            raise AttributeError

        def getAttributeSecure(self, name, default=None):
            if self.hasAttribute(name):
                return self.__dict__[name]
            return default

        def setAttribute(self, name, value):
            if name not in self._original_fields:
                self._original_fields.append(name)
                self._fields.append(_prefix_keyword(name, warn))
            self.__dict__[_prefix_keyword(name, warn)] = value

        def hasChild(self, name):
            return name in self._child_dict

        def getChild(self, name):
            return self._child_dict[name]

        def addChild(self, name, attrs=None):
            if attrs is None:
                attrs = {}
            clazz = compound_object(name, attrs.keys())
            child = clazz([attrs.get(a) for a in sorted(attrs.keys())])
            self._child_dict.setdefault(name, []).append(child)
            self._child_list.append(child)
            return child

        def removeChild(self, child):
            self._child_dict[child.name].remove(child)
            self._child_list.remove(child)

        def setChildList(self, childs):
            for c in self._child_list:
                self._child_dict[c.name].remove(c)
            for c in childs:
                self._child_dict.setdefault(c.name, []).append(c)
            self._child_list = childs

        def getChildList(self):
            return self._child_list

        def getText(self):
            return self._text

        def setText(self, text):
            self._text = text

        def __getattr__(self, name):
            if name[:2] != "__":
                return self._child_dict.get(name, None)
            raise AttributeError

        def __setattr__(self, name, value):
            if name != "_child_dict" and name in self._child_dict:
                # this could be optimized by using the child_list only if there are different children
                for c in self._child_dict[name]:
                    self._child_list.remove(c)
                self._child_dict[name] = value
                for c in value:
                    self._child_list.append(c)
            else:
                self.__dict__[name] = value

        def __delattr__(self, name):
            if name in self._child_dict:
                for c in self._child_dict[name]:
                    self._child_list.remove(c)
                del self._child_dict[name]
            else:
                if name in self.__dict__:
                    del self.__dict__[name]
                self._original_fields.remove(name)
                self._fields.remove(_prefix_keyword(name, False))

        def __getitem__(self, name):
            return self._child_dict[name]

        def __str__(self):
            nodeText = '' if self._text is None else ",text=%s" % self._text
            return "<%s,child_dict=%s%s>" % (self.getAttributes(), dict(self._child_dict), nodeText)

        def toXML(self, initialIndent="", indent="    "):
            fields = ['%s="%s"' % (self._original_fields[i], getattr(self, k))
                      for i, k in enumerate(self._fields) if getattr(self, k) is not None and
                      # see #3454
                      '{' not in self._original_fields[i]]
            if not self._child_dict and self._text is None:
                return initialIndent + "<%s %s/>\n" % (self.name, " ".join(fields))
            else:
                s = initialIndent + "<%s %s>\n" % (self.name, " ".join(fields))
                for c in self._child_list:
                    s += c.toXML(initialIndent + indent)
                if self._text is not None and self._text.strip():
                    s += self._text.strip(" ")
                return s + "%s</%s>\n" % (initialIndent, self.name)

        def __repr__(self):
            return str(self)

        def __lt__(self, other):
            return str(self) < str(other)

    return CompoundObject


def parse(xmlfile, element_names, element_attrs={}, attr_conversions={},
          heterogeneous=True, warn=False):
    """
    Parses the given element_names from xmlfile and yield compound objects for
    their xml subtrees (no extra objects are returned if element_names appear in
    the subtree) The compound objects provide all element attributes of
    the root of the subtree as attributes unless attr_names are supplied. In this
    case attr_names maps element names to a list of attributes which are
    supplied. If attr_conversions is not empty it must map attribute names to
    callables which will be called upon the attribute value before storing under
    the attribute name.
    The compound objects gives dictionary style access to list of compound
    objects o for any children with the given element name
    o['child_element_name'] = [osub0, osub1, ...]
    As a shorthand, attribute style access to the list of child elements is
    provided unless an attribute with the same name as the child elements
    exists (i.e. o.child_element_name = [osub0, osub1, ...])
    @Note: All elements with the same name must have the same type regardless of
    the subtree in which they occur (heterogeneous cases may be handled by
    setting heterogeneous=True (with reduced parsing speed)
    @Note: Attribute names may be modified to avoid name clashes
    with python keywords. (set warn=True to receive renaming warnings)
    @Note: The element_names may be either a single string or a list of strings.
    @Example: parse('plain.edg.xml', ['edge'])
    """
    if isinstance(element_names, str):
        element_names = [element_names]
    elementTypes = {}
    for _, parsenode in ET.iterparse(_open(xmlfile, None)):
        if parsenode.tag in element_names:
            yield _get_compound_object(parsenode, elementTypes,
                                       parsenode.tag, element_attrs,
                                       attr_conversions, heterogeneous, warn)
            parsenode.clear()


def _IDENTITY(x):
    return x


def _get_compound_object(node, elementTypes, element_name, element_attrs, attr_conversions, heterogeneous, warn):
    if element_name not in elementTypes or heterogeneous:
        # initialized the compound_object type from the first encountered #
        # element
        attrnames = element_attrs.get(element_name, node.keys())
        if len(attrnames) != len(set(attrnames)):
            raise Exception(
                "non-unique attributes %s for element '%s'" % (attrnames, element_name))
        elementTypes[element_name] = compound_object(
            element_name, attrnames, warn)
    # prepare children
    child_dict = {}
    child_list = []
    if len(node) > 0:
        for c in node:
            child = _get_compound_object(c, elementTypes, c.tag, element_attrs, attr_conversions, heterogeneous, warn)
            child_dict.setdefault(c.tag, []).append(child)
            child_list.append(child)
    attrnames = elementTypes[element_name]._original_fields
    return elementTypes[element_name](
        [attr_conversions.get(a, _IDENTITY)(node.get(a)) for a in attrnames],
        child_dict, node.text, child_list)


def create_document(root_element_name, attrs=None, schema=None):
    if attrs is None:
        attrs = {}
    if schema is None:
        attrs["xmlns:xsi"] = "http://www.w3.org/2001/XMLSchema-instance"
        attrs["xsi:noNamespaceSchemaLocation"] = "http://sumo.dlr.de/xsd/" + root_element_name + "_file.xsd"
    clazz = compound_object(root_element_name, sorted(attrs.keys()))
    return clazz([attrs.get(a) for a in sorted(attrs.keys())], OrderedDict())


def sum(elements, attrname):
    # for the given elements (as returned by method parse) compute the sum for attrname
    # attrname must be the name of a numerical attribute
    return reduce(lambda x, y: x + y, [float(getattr(e, attrname)) for e in elements])


def average(elements, attrname):
    # for the given elements (as returned by method parse) compute the average for attrname
    # attrname must be the name of a numerical attribute
    if elements:
        return sum(elements, attrname) / len(elements)
    else:
        raise Exception("average of 0 elements is not defined")


# def globalize(func):
#     def result(*args, **kwargs):
#         return func(*args, **kwargs)
# #   result.__name__ = result.__qualname__ = uuid.uuid4().hex
# #   setattr(sys.modules[result.__module__], result.__name__, result)
# #   return result
#     result.__name__ = result.__qualname__ = (
#     os.path.abspath(func.__code__.co_filename).replace('.', '') + '\0' +
#     str(func.__code__.co_firstlineno))
#     setattr(sys.modules[result.__module__], result.__name__, result)
#     return result


@contextmanager
def globalized(func):
    namespace = sys.modules[func.__module__]
    name, qualname = func.__name__, func.__qualname__
    func.__name__ = func.__qualname__ = f'_{name}_{uuid.uuid4().hex}'
    setattr(namespace, func.__name__, func)
    try:
        yield
    finally:
        delattr(namespace, func.__name__)
        func.__name__, func.__qualname__ = name, qualname


def simple_namespace_creator(keywords):
    # @globalize
    def create_named_tuple(*args, **kwargs):
        if args:
            kwargs = {key: arg for key, arg in zip(keywords, args)}
        return SimpleNamespace(**kwargs)
    return create_named_tuple


class RecordLike(Namespace):

    def __init__(self, **kwrgs):
        super().__init__(**kwrgs)
        self._ordered_kwargs = [key for key in kwrgs.keys()] 

    def __iter__(self, ):
        for item in self._ordered_kwargs:
            yield self.__dict__[item] 


def _createRecordAndPattern(element_name, attrnames, warn, optional,):
    if isinstance(attrnames, str):
        attrnames = [attrnames]
    prefixedAttrnames = [_prefix_keyword(a, warn) for a in attrnames]
    name = _prefix_keyword(element_name, warn)
    Record = namedtuple(name, prefixedAttrnames, module="__mp_main__")
    pickleable_record = prefixedAttrnames
    if optional:
        pattern = ''.join(['<%s' % element_name] +
                          ['(\\s+%s="(?P<%s>[^"]*?)")?' % a for a in zip(attrnames, prefixedAttrnames)])
    else:
        pattern = '.*'.join(['<%s' % element_name] +
                            ['%s="([^"]*)"' % attr for attr in attrnames])
    reprog = re.compile(pattern)
    
    # globals()[name].__qualname__ = name
    # import __main__
    # setattr(__main__, globals()[name].__name__, globals()[name])
    # Record.__module__ = "__main__"
    # Record.__qualname__ = 'Record'
    # globals()[_prefix_keyword(element_name, warn)] = Record
    
    # exec(f"global {name}")

    return Record, reprog, pickleable_record


def _open(xmlfile, encoding="utf8", raw=False):
    if isinstance(xmlfile, str):
        if xmlfile.endswith(".gz"):
            return gzip.open(xmlfile, "rt")
        if encoding is not None:
            return io.open(xmlfile, encoding=encoding, mode='r+' if raw else None)
    return xmlfile


def _unpacker(record_obj, optional_flag, re_match=None, grouped_obj=None):
    if optional_flag:
        return record_obj(**{re_match.groupdict() if re_match else grouped_obj})
    else:
        return record_obj(*(re_match.groups() if re_match else grouped_obj))


def parse_fast(xmlfile, element_name, attrnames, warn=False, optional=False, threads=None, encoding="utf8"):
    """
    Parses the given attrnames from all elements with element_name
    @Note: The element must be on its own line and the attributes must appear in
    the given order.
    @Example: parse_fast('plain.edg.xml', 'edge', ['id', 'speed'])
    """
    Record, reprog = _createRecordAndPattern(element_name, attrnames, warn, optional)
    if not threads:
        for line in _open(xmlfile, encoding):
            m = reprog.search(line)
            if m:
                yield _unpacker(Record, optional, m)
    else:
        ThreadedXMLParser(
            file_obj=_open(xmlfile, encoding, raw=True),
            thread_num=threads,
            records=(Record, ),
            reprogs=(reprog, ),
            optional=optional
        )




def parse_fast_nested(xmlfile, element_name, attrnames, element_name2, attrnames2,
                      warn=False, optional=False, threads=None, encoding="utf8"):
    """
    Parses the given attrnames from all elements with element_name
    And attrnames2 from element_name2 where element_name2 is a child element of element_name
    @Note: The element must be on its own line and the attributes must appear in
    the given order.
    @Example: parse_fast_nested('fcd.xml', 'timestep', ['time'], 'vehicle', ['id', 'speed', 'lane']):
    """
    Record, reprog, pickle_record = _createRecordAndPattern(element_name, attrnames, warn, optional, )
    Record2, reprog2, pickle_record_2 = _createRecordAndPattern(element_name2, attrnames2, warn, optional)
    # globals()[name] = tmp
    # globals()[name_2] = tmp2
    record = None
    if threads:
        # mp.set_start_method("spawn", force=True)
        q = mp.Queue()

        # ThreadedXMLParser.set_records([globals()[name], globals()[name_2]])
        # with globalized(pickle_record), globalized(pickle_record_2):

        threaded_worker = ThreadedXMLParser(
                q=q,
                file_obj=xmlfile,
                thread_num=threads,
                reprogs=([element_name, attrnames], [element_name2, attrnames2]),
                records=(pickle_record, pickle_record_2),
                encoding=encoding,
                optional=optional
            )

            # threaded_worker.set_records([globals()[name], globals()[name_2]])
        threaded_worker.run()
        # p = mp.Process(target=threaded_worker.run, )
        
        # p.start()
        # threaded_worker.start()            
        iterator = ThreadedXMLParser.queue_iterator(threaded_worker.q)
    else:
        iterator = _open(xmlfile, encoding)
    
    for line in iterator:
        # t0 = time.time()
        if not threads:
            m2 = reprog2.search(line)
            m2 = _unpacker(Record2, optional, m2)
        else:
            m2 = line[1]
            record = line[0]
            # record = {'time': 0}
        if record and m2:
            yield record, m2
        else:
            m = reprog.search(line)
            if m:
                record = _unpacker(Record, optional, line[0])
            elif element_name in line:
                record = None
        
def writeHeader(outf, script=None, root=None, schemaPath=None, rootAttrs="", options=None):
    """
    Writes an XML header with schema information and a comment on how the file has been generated
    (script name, arguments and datetime). Please use this as first call whenever you open a
    SUMO related XML file for writing from your script.
    If script name is not given, it is determined from the command line call.
    If root is not given, no root element is printed (and thus no schema).
    If schemaPath is not given, it is derived from the root element.
    If rootAttrs is given as a string, it can be used to add further attributes to the root element.
    If rootAttrs is set to None, the schema related attributes are not printed.
    """
    if script is None or script == "$Id$":
        script = os.path.basename(sys.argv[0])
    if options is None:
        optionString = "  options: %s" % (' '.join(sys.argv[1:]).replace('--', '<doubleminus>'))
    else:
        optionString = options.config_as_string

    outf.write(u"""<?xml version="1.0" encoding="UTF-8"?>
<!-- generated on %s by %s %s
%s
-->
""" % (datetime.datetime.now(), script, version.gitDescribe(), optionString))
    if root is not None:
        if rootAttrs is None:
            outf.write((u'<%s>\n') % root)
        else:
            if schemaPath is None:
                schemaPath = root + "_file.xsd"
            outf.write((u'<%s%s xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" ' +
                        u'xsi:noNamespaceSchemaLocation="http://sumo.dlr.de/xsd/%s">\n') %
                       (root, rootAttrs, schemaPath))


def quoteattr(val):
    # saxutils sometimes uses single quotes around the attribute
    # we can prevent this by adding an artificial single quote to the value and removing it again
    return '"' + xml.sax.saxutils.quoteattr("'" + val)[2:]


class ThreadedXMLParser: #(mp.Process):

    TABLE_REPLACE = str.maketrans(dict.fromkeys('\r\n\t"'))

    def __init__(self, file_obj, q, thread_num, records, reprogs, sorted=True, encoding='utf', optional=False):
        # mp.Process.__init__(self)
        self.q = q
        self._file_obj = file_obj # mmap.mmap(file_obj.fileno(), 0)
        self._records = records # [globals()[rec] for rec in records]
        # preprocessing
        self._reprogs = [[re, [[prog, len(prog) + 1] for prog in progs]] for re, progs in reprogs]
        self._2_depth = True if len(self._reprogs) > 1 else False
        self._threads = thread_num
        self._sorted = sorted
        self._encoding = encoding
        # self._chunk_size = self._data.size() // (self._threads * 4)
        self._optional = optional
        self.daemon = False


    @classmethod
    def set_records(cls, records):
        cls.records = records  # [globals()[rec] for rec in records]

    # @classmethod
    def run(self, ):
        # import __main__
        # getattr(__main__, self._records[0])
        self.parse()
    

    @staticmethod
    def queue_iterator(q_obj):
         while True:
            try:
                obj = q_obj.get(block=False, timeout=1e-6)
                if obj == -1:
                    break
                yield from obj[-1]
            except Empty:
                pass


    def _chunker(self, data):
        # the file hasn't been decoded yet, can only decode in chuncks
        
        for m in re.finditer(self._reprogs[0][0].encode(self._encoding), data):
            yield m.start()
            # last = m.start()
    
    def _grouper(self, data):
        chunk_size = data.size() // (self._threads * 4)
        start_ind = 0
        for i, chunk_ind in enumerate(self._chunker(data)):
            if ((chunk_ind - start_ind) >= chunk_size):
                # print("yielding", start_ind, chunk[0])
                yield data[start_ind:chunk_ind]
                start_ind = chunk_ind
        # The last chunk                
        yield data[start_ind:chunk_ind]

    @staticmethod
    def _clean_match(match, reprogs):
        # cleaned = match.translate(ThreadedXMLParser.TABLE_REPLACE)
        # cleaned = match[1:match.find('/>')]
        cleaned=match
        final = []
        i = 0
        for v in cleaned.split(' '):
            split_v = v.split("=")
            if split_v[0] in reprogs[1]:
                # final[split_v[0]] = split_v[-1].replace('>', '')
                final.append(split_v[-1].replace('>', ''))
                i += 1
            if i >= len(reprogs[1]):
                break
        return final

    @staticmethod
    def _parser(chunk, thread_num, q, encoding, records, reprogs, depth_2, optional, top_record=None, res=[], record_fns=None):

        
        chunk = chunk.decode(encoding)

        iterator = chunk.translate(ThreadedXMLParser.TABLE_REPLACE).split(reprogs[0][0])[1:]
        # if level < 1:
        iterator = iterator[1::2]
        
        if not len(iterator):
            return 
        # last_m = iterator[0] 
        first_level = None
        res = []
        for m in iterator:
            # m.rfind('/>')
            if reprogs[0][0][0] in chunk:
                first_level = []
                for v in m.split(' '):
                    split_v = v.split("=")
                    # if split_v[0] in reprogs[1]:
                        # final[split_v[0]] = split_v[-1].replace('>', '')
                    first_level.append(split_v[-1].replace('>', ''))
                    #     # i += 1
                    # if i >= len(reprogs[1]):
                    #     break
                
                # first_level = ThreadedXMLParser._clean_match(m, reprogs[0])
                # first_level = first_level RecordLike(**first_level)
                if not depth_2:
                    res.append(first_level)

            if depth_2 and first_level:
                for inner_m in m.split(reprogs[1][0])[1:]:
                    # if reprogs[1][0] in inner_m:
                    # second_level = ThreadedXMLParser._clean_match(inner_m, reprogs[1])
                    # second_level = RecordLike(**second_level)
                    second_level = []
                    last = 0
                    for spliter, len_splitter in reprogs[1][1]:
                        location = inner_m[last:].find(spliter)
                        reduced_s = inner_m[last + location + len_splitter:] 
                        val = reduced_s[:reduced_s.find(' ')]
                        second_level.append(val)
                        last = location



                    # for v in inner_m.split(' '):
                    #     split_v = v.split("=")
                    #     if split_v[0] in reprogs[1][1]:
                    #         second_level.append(split_v[1])
                    # second_level = [inner_m.split("=")[1] [1:]] # if inner_m.split("=")[0] in reprogs[1][1]]
                    if second_level:
                        res.append((first_level, second_level))

            # if level < 1 and depth_2:

            #     parent = ThreadedXMLParser._clean_match(last_m, reprogs[0])

            #     ThreadedXMLParser._parser(
            #         chunk=m,
            #         thread_num=thread_num,
            #         q=q,
            #         encoding=encoding,
            #         reprogs=[reprogs[1]],
            #         records=[records[1]],
            #         depth_2=depth_2,
            #         optional=optional,
            #         level=level + 1,
            #         top_record=parent,  #last_m.groupdict() if optional else last_m.groups(),
            #         res=res,
            #         record_fns=record_fns
            #     )
            
            # if level >= 1:
            #     cleaned = ThreadedXMLParser._clean_match(m, reprogs[0])
            #     # res.append((top_record, RecordLike(**cleaned)))  # if optional else RecordLike(**{key: _x for key, _x in zip(records[1], m.groups())})))
            #     res.append((cleaned))


            # elif not depth_2:
            #     cleaned = ThreadedXMLParser._clean_match(m, reprogs[0])
            #     res.append((cleaned))  # if optional else RecordLike(**{key: _x for key, _x in zip(records[0], m.groups())})))
            
            # if depth_2 and level < 1:
            #     last_m = m

        # if level < 1:
        # q.put((thread_num, res))
        print("finished ", res[0][0])
            # return thread_num, res    

    def parse(self,  **kwargs):

        # manager = mp.Manager()
        # q = manager.Queue()
        p = []

        f = _open(self._file_obj, self._encoding, raw=True)

        chunker = self._grouper(
            mmap.mmap(
                f.fileno(), 
                0
                )
            )
        i = 0
        # temporary holder so that items can be returned in order
        # results = []
        # desired_yield = 0
        # cleanup = False

        # partial_func = functools.partial(
        #         self._parser, 
        #         # chunk=next(chunker), 
        #         thread_num=i, 
        #         q=self.q,
        #         optional=self._optional,
        #         encoding=self._encoding, 
        #         reprogs=self._reprogs, 
        #         depth_2=self._2_depth
        #     ) 

        # ps = []
        # iterfunc = zip(partial_func, chunker)
        stop = False
        s = []
        while True:
            try:
                # print(next(chunker)[:5])
                # # s[-1] = mp.Event()
                print("spawning process")
                p.append(mp.Process(target=self._parser,
                                    kwargs=dict(
                                                chunk=next(chunker), 
                                                thread_num=i, 
                                                q=self.q,
                                                optional=self._optional,
                                                encoding=self._encoding,
                                                records=self._records, 
                                                reprogs=self._reprogs, 
                                                depth_2=self._2_depth)
                                        )
                ) 
                p[-1].daemon = False
                print("created")
                p[-1].start()
                p[-1].join(timeout=0)

            except StopIteration:
                stop = True

            while (len(p) >= self._threads) or (stop and len(p)):
                p = [_p for _p in p if _p.is_alive()]
                time.sleep(0.01)
 
            if stop:
                break
        
        self.q.put(-1)

        # with mp.Pool(self._threads) as pool:
        #     for res in pool.map(partial_func, chunker, ):
        #         print(res[-1][0][0])
        #         yield from res[-1]
                # yield 
        # while True:
        #     try:
        #         p.append(
        #             mp.Process(
        #                 target=self._parser, 
        #                 kwargs=dict(
        #                     chunk=next(chunker), 
        #                     thread_num=i, 
        #                     q=q,
        #                     optional=self._optional,
        #                     encoding=self._encoding, 
        #                     reprogs=self._reprogs, 
        #                     depth_2=self._2_depth)
        #             )
        #         )
        #         p[-1].start()
        #         p[-1].join(timeout=0)
        #     except StopIteration:
        #         cleanup = True

        #     # only start checking for results once max threads spooled up
        #     while (len(p) >= self._threads) or (cleanup and len(p)):
        #         try:
        #             q_res = q.get(block=False, )
        #             p.pop(q_res[0])
        #             if sorted:
        #                 results.append(q_res)
        #                 # try to yield the next sequential item
        #                 for j, item in enumerate(results):
        #                     if item[0] == desired_yield:
        #                         desired_yield += 1
        #                         # check the queue again
        #                         break
        #                 yield from results.pop(j)[1]
        #             else:
        #                 yield from q_res[1]

        #         except (TimeoutError, Empty):
        #             pass
        #     # exit condition
        #     if cleanup and not len(p):
        #         break
        #         # # last_yield
        #         # if q_res[0] == last_yield + 1:
        #         #     results.append(q_res)
        #         # else:

        #         # else:
        #         #     yield results.pop(q_res)
        #     i += 1


    # def _puller(self, ):

    #     def puller(q, ):

    #         try:
    #             item = q.get(block=True, timeout=0.001)
    #             # f.writelines(item)
    #         except Empty:
    #             continue
            
    #         # exit condition
    #         if e.is_set() and q.empty():
    #             break

            
    #     return puller




# class EmissionsXML2CSV:

#     """
#     Example usage: 

#         e = EmissionsXML2CSV("/home/max/tmp/_OUTPUT_emissions.xml", 20)
#         e.convert("/home/max/tmp/_OUTPUT_emissions.csv")

    
#     Using 20 CPU cores, it reduces the time to process one 1.5 Gb emissions 
#     output xml from ~240 seconds with $SUMO_HOME/tools/xml/xml2csv.py to ~8 seconds
#     ~

#     """
#     TABLE_REPLACE = str.maketrans(dict.fromkeys('\r\n\t"'))
#     TIME_PATTERN = re.compile(r'time="[\d.]+"')
#     HEADER_ROW = ['timestep_time',
#                    'vehicle_id',
#                    'vehicle_eclass',
#                    'vehicle_CO2',
#                    'vehicle_CO',
#                    'vehicle_HC',
#                    'vehicle_NOx',
#                    'vehicle_PMx',
#                    'vehicle_fuel',
#                    'vehicle_electricity',
#                    'vehicle_noise',
#                    'vehicle_route',
#                    'vehicle_type',
#                    'vehicle_waiting',
#                    'vehicle_lane',
#                    'vehicle_pos',
#                    'vehicle_speed',
#                    'vehicle_angle',
#                    'vehicle_x',
#                    'vehicle_y']

#     def __init__(self, path_2_xml, num_core, chunk_size=None):

#         self._data = self._mmap_xml(path_2_xml)
#         self.cpu = num_core

#         # 4x cores seems to be the best setting here
#         self._chunk_size = self._data.size() // (self.cpu * 4) if not chunk_size else chunk_size

#     def _mmap_xml(self, path_2_xml: str) -> mmap.mmap:
#         with open(path_2_xml, 'r+') as f:
#             return mmap.mmap(f.fileno(), 0)

#     def _chunk_file(self, ) -> Iterable[int]:
#         pattern = br"time="
#         last = 0
#         for m in re.finditer(pattern, self._data):
#             yield last, m.start()
#             last = m.start()

#     def _grouper(self, n,):
#         # chunks = self._split_list(list(self._chunk_file()), n)
#         start_ind = 0
#         for i, chunk in enumerate(self._chunk_file()):
#             if ((chunk[0] - start_ind) >= self._chunk_size):
#                 # print("yielding", start_ind, chunk[0])
#                 yield self._data[start_ind:chunk[0]]
#                 start_ind = chunk[0]
       
#         # The last chunk                
#         yield self._data[start_ind:chunk[0]]


#     @staticmethod
#     def _chunk_handler(chunk, q: mp.Queue, *args, **kwargs):
#         # data = []
#         chunk = chunk.decode()
#         vehicle_chunk = "<vehicle"
#         iter_finder = EmissionsXML2CSV.TIME_PATTERN.finditer(chunk)
#         time_obj = next(iter_finder)
#         d = []
#         # print("chunk start time: ", time_obj.group()[6:-1])
#         for next_time in [*iter_finder, None]:
#             local_time = time_obj.group()[6:-1]
#             end = -1 if not next_time else next_time.span()[0]
#             time_chunk = chunk[time_obj.span()[1]:end]
#             if vehicle_chunk in time_chunk:
#                 for vehicle in time_chunk.split(vehicle_chunk):
#                     if 'id' in vehicle:
#                         cleaned = vehicle.translate(EmissionsXML2CSV.TABLE_REPLACE)
#                         cleaned = cleaned[1:cleaned.find('/>')]
#                         parsed = [v.split("=")[1] for v in cleaned.split(' ')]
#                         d.append(",".join([local_time] + parsed) + "\n"
#                         )
#             if end > 0:
#                 time_obj = next_time
#         # print("chunk end time: ", time_obj.group()[6:-1])
#         q.put(d)

#     @staticmethod
#     def _q_consumer(output_csv_path: str, q: mp.Queue, e: mp.Event) -> None:
#         with open(output_csv_path, 'w', newline='', encoding='utf-8') as f:
#             # writer = csv.writer(f) TOO SLOW
#             # write the header
#             f.writelines([",".join(EmissionsXML2CSV.HEADER_ROW) + "\n"])

#             while True:
#                 try:
#                     item = q.get(block=True, timeout=0.001)
#                     f.writelines(item)
#                 except Empty:
#                     continue
                
#                 # exit condition
#                 if e.is_set() and q.empty():
#                     break

#     def convert(self, path_2_csv) -> None:
#         manager = mp.Manager()
#         q = manager.Queue()
#         s = manager.Event()
#         writer_p = mp.Process(target=self._q_consumer, kwargs=dict(output_csv_path=path_2_csv, q=q, e=s))
#         writer_p.start()

#         p = []
#         chunker = self._grouper(self.cpu)
#         while True:
#         # for i, g in enumerate(self._grouper(self.cpu - 2)):
#             try:
#                 p.append(mp.Process(target=self._chunk_handler, kwargs=dict(chunk=next(chunker), q=q)))
#                 p[-1].start()
#                 p[-1].join(timeout=0)
#                 cleanup = False
#             except StopIteration:
#                 cleanup = True

#             while True:
                
#                 alive_p = [(i, _p.is_alive()) for i, _p in enumerate(p)]

#                 if ((
#                         any(not alive for _, alive in alive_p)
#                         or len(p) < (self.cpu)
#                     ) and not cleanup) or (   
#                         cleanup and all(not alive for _, alive in alive_p)
#                     ):

#                     break
#                 time.sleep(0.001)

#             p = [p[i] for i, alive in alive_p if alive]

#             if cleanup:
#                 # kill the writer
#                 s.set()
#                 writer_p.join()
                
#                 # EXIT
#                 break


if __name__ == "__main__":

    
    file_path = r"C:\Users\gle\Desktop\_OUTPUT_emissions.xml"


    with open(r"C:\Users\gle\Desktop\_OUTPUT_emissions.csv", 'w') as f:
        f.writelines(
            (
                "".join((",".join([line[0]['time'], *list(line[1].values())]), "\n")) for line in parse_fast_nested(
                    file_path,
                    'timestep',
                    ['time'],
                    "vehicle",
                    ['id',
                    'eclass',
                    'CO2',
                    'CO',
                    'HC',
                    'NOx',
                    'PMx',
                    'fuel',
                    'electricity',
                    'noise',
                    'route',
                    'type',
                    'waiting',
                    'lane',
                    'pos',
                    'speed',
                    'angle',
                    'x',
                    'y'
                    ],
                    threads=8
                )
            )
        )
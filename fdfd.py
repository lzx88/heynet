# -*- coding: utf-8 -*-
import sys
import codecs
import xlrd
import os
import shutil

def setKey(dct, k):
    if dct.get(k) == None:
        dct[k] = {}
    return dct[k]

def text(val, dim = '"'):
    if type(val) is str or type(val) is unicode:
        if len(val) > 0 and val[0] == '{':
            return val
        return "%s%s%s" % (dim, val, dim)
    return val

class ExcelConverter:
    def __init__(self, path, fileName):
        self.fileName = fileName
        fullPath = os.path.join(path, fileName)
        self.workBook = xlrd.open_workbook(fullPath)

    def cell(self, sheet, row, col):
        val = sheet.cell_value(row, col)
        if type(val) == float:
            return int(val)
        return val

    def toDict(self, sheet):
        dct = {}
        kn = self.cell(sheet, 0, 0)
        if kn == "" or int(kn) < 1:
            kn = 1
        assert (kn < 4 and kn < sheet.ncols)
        for r in range(3, sheet.nrows):
            dt = dct
            for c in range(0, kn):
                dt = setKey(dt, self.cell(sheet, r, c))
            for c in range(0, sheet.ncols):
                cell = [self.cell(sheet, 2, c), self.cell(sheet, r, c), self.cell(sheet, 1, c), self.cell(sheet, 0, c)]
                dt[c] = cell
        return dct
        
         def textLua(self, key, val, tab = ""):
        if type(val) is dict:
            key = text(key)
            self.lines.append('%s[%s] = {'%(tab, key))
            for k in val:
                self.textLua(k, val[k], '%s  '%(tab))
            self.lines.append(tab == "" and "}" or "%s},"%(tab))
        elif type(val) is list:
            flag = str.lower(str(val[3]))
            if(self.mode == "s" and not flag == "c") or (self.mode == "c" and not flag == "s"):
                self.lines.append('%s[%s] = %s, --%s'%(tab, text(val[0]), text(val[1]), val[2]))

    def textJson(self, key, val, tab=""):
        if type(val) is dict:
            key = text(key)
            self.lines.append('%s"%s" : {' % (tab, key))
            for k in val:
                self.textJson(k, val[k], '%s  ' % (tab))
            self.lines.append(tab == "" and "}" or "%s}," % (tab))
        elif type(val) is list:
            flag = str.lower(str(val[3]))
            if (self.mode == "s" and not flag == "c") or (self.mode == "c" and not flag == "s"):
                self.lines.append('%s"%s" : %s, //%s' % (tab, val[0], text(val[1]), val[2]))
                
def convert(self, desPath, textType, mode, ext):
        self.mode = str.lower(mode)
        for sht in self.workBook.sheets():
            dct = self.toDict(self.workBook.sheet_by_name(sht.name))
            self.lines = []
            if textType == "lua":
                for k in dct:
                    self.textLua(k, dct[k])
            elif textType == "json":
                self.lines.append("{")
                for k in dct:
                    self.textJson(k, dct[k], "  ")
                self.lines.append("}")
            fullPath = os.path.abspath(os.path.join(desPath, sht.name) + ext)
            f = codecs.open(fullPath, "w", "utf-8")
            for l in self.lines:
                f.write("%s\n"%(l))
            f.close()
            print "Convert %s file success:\t%s" % (textType, fullPath)

if __name__ == "__main__":
    excel_root_path = os.path.abspath("../xls")
    #target = "lua" # lua or json
    topath = "./"#sys.argv[1] # "../Web/excel-config"
    #mode = 'S'#sys.argv[2]  # C or S
    #ext = ".json"
    allxls = os.listdir(excel_root_path)
    for xlsname in allxls:
        if ".xlsx" not in xlsname:
            continue
        print(os.path.join(excel_root_path, xlsname) + " ...")
        ex = ExcelConverter(excel_root_path, xlsname)
        ex.convert(topath, "lua", 'C', ".lua")
        
     for xlsname in allxls:
        if ".xlsx" not in xlsname:
            continue
        print(os.path.join(excel_root_path, xlsname) + " ...")
        ex = ExcelConverter(excel_root_path, xlsname)
        ex.convert(topath, "lua", 'S', ".tmp~")
   

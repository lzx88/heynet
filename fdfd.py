func WalkFile(root, suffix string, ls func(string, os.FileInfo)) error  {
	return filepath.Walk(root, func(path string, info os.FileInfo, err error) error {
		if info == nil || info.IsDir() {
			return err
		}
		if strings.HasSuffix(path, suffix) {
			ls(path, info)
		}
		return nil
	})
}




package config

import (
	"github.com/gopher-lua"
)

type config map[string] *Val

func (the config)Find(t string, keys... string) *Val {
	v := the[t]
	if v == nil{
		panic("config no exist")
	}
	for _, k := range keys{
		v = v.Get(k)
	}
	return v
}

type Val struct {
	v lua.LValue
}

func (the *Val)Get(k string) *Val {
	tbl, ok := the.v.(*lua.LTable)
	if !ok {
		panic("config is no a table")
	}
	return &Val{tbl.RawGet(lua.LString(k))}
}

func (the *Val)ToUint32() uint32 {
	return uint32(lua.LVAsNumber(the.v))
}
func (the *Val)ToString() string {
	return lua.LVAsString(the.v)
}
func (the *Val)ToFloat() float64 {
	return float64(lua.LVAsNumber(the.v))
}
func (the *Val)ToInt32() int32 {
	return int32(lua.LVAsNumber(the.v))
}


package config

import (
	"github.com/gopher-lua"
	"io/ioutil"
	"base"
	"os"
	"strings"
)

func assert(err error) {
	if err != nil {
		panic(err)
	}
}

func New(root string) config{
	L := lua.NewState()
	defer L.Close()
	c := make(map[string] *Val)
	base.WalkFile(root, ".lua", func(path string, info os.FileInfo) {
		text, err := ioutil.ReadFile(path)
		assert(err)
		err = L.DoString(string(text))
		assert(err)
		lv := L.Get(-1) // get the value at the top of the stack
		tbl, ok := lv.(*lua.LTable)
		if !ok {
			panic("config is no a table")
		}
		name := strings.ToLower(strings.TrimSuffix(info.Name(), ".lua"))
		c[name] = &Val{tbl}
	})
	return c
}


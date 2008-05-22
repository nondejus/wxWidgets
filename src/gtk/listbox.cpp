/////////////////////////////////////////////////////////////////////////////
// Name:        src/gtk/listbox.cpp
// Purpose:
// Author:      Robert Roebling
// Modified By: Ryan Norton (GtkTreeView implementation)
// Id:          $Id$
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_LISTBOX

#include "wx/listbox.h"

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/settings.h"
    #include "wx/checklst.h"
    #include "wx/arrstr.h"
#endif

#include "wx/gtk/private.h"
#include "wx/gtk/treeentry_gtk.h"

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

//-----------------------------------------------------------------------------
// data
//-----------------------------------------------------------------------------

extern bool           g_blockEventsOnDrag;
extern bool           g_blockEventsOnScroll;



//-----------------------------------------------------------------------------
// Macro to tell which row the strings are in (1 if native checklist, 0 if not)
//-----------------------------------------------------------------------------

#if wxUSE_CHECKLISTBOX
#   define WXLISTBOX_DATACOLUMN_ARG(x)  (x->m_hasCheckBoxes ? 1 : 0)
#else
#   define WXLISTBOX_DATACOLUMN_ARG(x)  (0)
#endif // wxUSE_CHECKLISTBOX

#define WXLISTBOX_DATACOLUMN    WXLISTBOX_DATACOLUMN_ARG(this)

//-----------------------------------------------------------------------------
// "row-activated"
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_listbox_row_activated_callback(GtkTreeView        * WXUNUSED(treeview),
                                   GtkTreePath        *path,
                                   GtkTreeViewColumn  * WXUNUSED(col),
                                   wxListBox          *listbox)
{
    if (g_blockEventsOnDrag) return;
    if (g_blockEventsOnScroll) return;

    // This is triggered by either a double-click or a space press

    int sel = gtk_tree_path_get_indices(path)[0];

    wxCommandEvent event(wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, listbox->GetId() );
    event.SetEventObject( listbox );

    if (listbox->IsSelected(sel))
    {
        GtkTreeEntry* entry = listbox->GtkGetEntry(sel);

        if (entry)
        {
            event.SetInt(sel);
            event.SetString(wxConvUTF8.cMB2WX(gtk_tree_entry_get_label(entry)));

            if ( listbox->HasClientObjectData() )
                event.SetClientObject( (wxClientData*) gtk_tree_entry_get_userdata(entry) );
            else if ( listbox->HasClientUntypedData() )
                event.SetClientData( gtk_tree_entry_get_userdata(entry) );

            g_object_unref (entry);
        }
        else
        {
            wxLogSysError(wxT("Internal error - could not get entry for double-click"));
            event.SetInt(-1);
        }
    }
    else
    {
        event.SetInt(-1);
    }

    listbox->HandleWindowEvent( event );
}
}

//-----------------------------------------------------------------------------
// "changed"
//-----------------------------------------------------------------------------

extern "C" {
static void
gtk_listitem_changed_callback(GtkTreeSelection * WXUNUSED(selection),
                              wxListBox *listbox )
{
    if (g_blockEventsOnDrag) return;

    wxCommandEvent event(wxEVT_COMMAND_LISTBOX_SELECTED, listbox->GetId() );
    event.SetEventObject( listbox );

    if (listbox->HasFlag(wxLB_MULTIPLE) || listbox->HasFlag(wxLB_EXTENDED))
    {
        wxArrayInt selections;
        listbox->GetSelections( selections );
        
        if ((selections.GetCount() == 0) && (listbox->m_oldSelection.GetCount() == 0))
        {
            // nothing changed, just leave
            return;
        }
        
        if (selections.GetCount() == listbox->m_oldSelection.GetCount())
        {
            bool changed = false;
            size_t idx;
            for (idx = 0; idx < selections.GetCount(); idx++)
            {
                if (selections[idx] != listbox->m_oldSelection[idx])
                {
                    changed = true;
                    break;
                }
            }
            
            // nothing changed, just leave
            if (!changed)
               return;
        }

        if (selections.GetCount() == 0)
        {
            // indicate that this is a deselection
            event.SetExtraLong( 0 );
            
            // take first item in old selection
            event.SetInt( listbox->m_oldSelection[0] );
            event.SetString( listbox->GetString( listbox->m_oldSelection[0] ) );
            
            listbox->m_oldSelection = selections;

            listbox->HandleWindowEvent( event );

            return;
        }
        
        // Now test if any new item is selected
        bool any_new_selected = false;
        size_t idx;
        for (idx = 0; idx < selections.GetCount(); idx++)
        {
            int item = selections[idx];
            if (listbox->m_oldSelection.Index(item) == wxNOT_FOUND)
            {
                event.SetInt( item );
                event.SetString( listbox->GetString( item ) );
                any_new_selected = true;
                break;
            }
        }
        
        if (any_new_selected)
        {
            // indicate that this is a selection
            event.SetExtraLong( 1 );

            listbox->m_oldSelection = selections;
            listbox->HandleWindowEvent( event );
            return;
        }
        
        // Now test if any new item is deselected
        bool any_new_deselected = false;
        for (idx = 0; idx < listbox->m_oldSelection.GetCount(); idx++)
        {
            int item = listbox->m_oldSelection[idx];
            if (selections.Index(item) == wxNOT_FOUND)
            {
                event.SetInt( item );
                event.SetString( listbox->GetString( item ) );
                any_new_deselected = true;
                break;
            }
        }
        
        if (any_new_deselected)
        {
            // indicate that this is a selection
            event.SetExtraLong( 0 );

            listbox->m_oldSelection = selections;
            listbox->HandleWindowEvent( event );
            return;
        }
        
        wxLogError( wxT("Wrong wxListBox selection") );
    }
    else
    {
        int index = listbox->GetSelection();
        if (index == wxNOT_FOUND)
        {
            // indicate that this is a deselection
            event.SetExtraLong( 0 );
            event.SetInt( -1 );

            listbox->HandleWindowEvent( event );

            return;
        }
        else
        {
            GtkTreeEntry* entry = listbox->GtkGetEntry( index );

            // indicate that this is a selection
            event.SetExtraLong( 1 );

            event.SetInt( index );
            event.SetString(wxConvUTF8.cMB2WX(gtk_tree_entry_get_label(entry)));

            if ( listbox->HasClientObjectData() )
                event.SetClientObject(
                    (wxClientData*) gtk_tree_entry_get_userdata(entry)
                                 );
            else if ( listbox->HasClientUntypedData() )
                event.SetClientData( gtk_tree_entry_get_userdata(entry) );

            listbox->HandleWindowEvent( event );

            g_object_unref (entry);
        }
    }
}
}

//-----------------------------------------------------------------------------
// "key_press_event"
//-----------------------------------------------------------------------------

extern "C" {
static gint
gtk_listbox_key_press_callback( GtkWidget *WXUNUSED(widget),
                                GdkEventKey *gdk_event,
                                wxListBox *listbox )
{
    if ((gdk_event->keyval == GDK_Return) || 
        (gdk_event->keyval == GDK_ISO_Enter) ||
        (gdk_event->keyval == GDK_KP_Enter))
    {
        int index = listbox->GetSelection();
        if (index != wxNOT_FOUND)
        {
        
            wxCommandEvent event(wxEVT_COMMAND_LISTBOX_DOUBLECLICKED, listbox->GetId() );
            event.SetEventObject( listbox );
            
            GtkTreeEntry* entry = listbox->GtkGetEntry( index );

            // indicate that this is a selection
            event.SetExtraLong( 1 );

            event.SetInt( index );
            event.SetString(wxConvUTF8.cMB2WX(gtk_tree_entry_get_label(entry)));

            if ( listbox->HasClientObjectData() )
                event.SetClientObject(
                    (wxClientData*) gtk_tree_entry_get_userdata(entry)
                                 );
            else if ( listbox->HasClientUntypedData() )
                event.SetClientData( gtk_tree_entry_get_userdata(entry) );

            /* bool ret = */ listbox->HandleWindowEvent( event );

            g_object_unref (entry);
            
//          wxMac and wxMSW always invoke default action
//          if (!ret)
            {
                // DClick not handled -> invoke default action
                wxWindow *tlw = wxGetTopLevelParent( listbox );
                if (tlw)
                {
                    GtkWindow *gtk_window = GTK_WINDOW( tlw->GetHandle() );
                    if (gtk_window)
                        gtk_window_activate_default( gtk_window );
                }
            }
            
            // Always intercept, otherwise we'd get another dclick
            // event from row_activated
            return TRUE;
        }
    }
    
    return FALSE;
}
}

//-----------------------------------------------------------------------------
// GtkTreeEntry destruction (to destroy client data)
//-----------------------------------------------------------------------------

extern "C" {
static void gtk_tree_entry_destroy_cb(GtkTreeEntry* entry,
                                      wxListBox* listbox)
{
    if (listbox->HasClientObjectData())
    {
        gpointer userdata = gtk_tree_entry_get_userdata(entry);
        if (userdata)
            delete (wxClientData *)userdata;
    }
}
}

//-----------------------------------------------------------------------------
// Sorting callback (standard CmpNoCase return value)
//-----------------------------------------------------------------------------

extern "C" {
static gint gtk_listbox_sort_callback(GtkTreeModel * WXUNUSED(model),
                                      GtkTreeIter  *a,
                                      GtkTreeIter  *b,
                                      wxListBox    *listbox)
{
    GtkTreeEntry* entry;
    GtkTreeEntry* entry2;

    gtk_tree_model_get(GTK_TREE_MODEL(listbox->m_liststore),
                             a,
                             WXLISTBOX_DATACOLUMN_ARG(listbox),
                             &entry, -1);
    gtk_tree_model_get(GTK_TREE_MODEL(listbox->m_liststore),
                             b,
                             WXLISTBOX_DATACOLUMN_ARG(listbox),
                             &entry2, -1);
    wxCHECK_MSG(entry, 0, wxT("Could not get entry"));
    wxCHECK_MSG(entry2, 0, wxT("Could not get entry2"));

    //We compare collate keys here instead of calling g_utf8_collate
    //as it is rather slow (and even the docs reccommend this)
    int ret = strcmp(gtk_tree_entry_get_collate_key(entry),
                     gtk_tree_entry_get_collate_key(entry2));

    g_object_unref (entry);
    g_object_unref (entry2);

    return ret;
}
}

//-----------------------------------------------------------------------------
// Searching callback (TRUE == not equal, FALSE == equal)
//-----------------------------------------------------------------------------

extern "C" {
static gboolean gtk_listbox_searchequal_callback(GtkTreeModel * WXUNUSED(model),
                                                 gint WXUNUSED(column),
                                                 const gchar* key,
                                                 GtkTreeIter* iter,
                                                 wxListBox* listbox)
{
    GtkTreeEntry* entry;

    gtk_tree_model_get(GTK_TREE_MODEL(listbox->m_liststore),
                             iter,
                             WXLISTBOX_DATACOLUMN_ARG(listbox),
                             &entry, -1);
    wxCHECK_MSG(entry, 0, wxT("Could not get entry"));
    wxGtkString keycollatekey(g_utf8_collate_key(key, -1));

    int ret = strcmp(keycollatekey,
                     gtk_tree_entry_get_collate_key(entry));

    g_object_unref (entry);

    return ret != 0;
}
}

//-----------------------------------------------------------------------------
// wxListBox
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxListBox, wxControlWithItems)

// ----------------------------------------------------------------------------
// construction
// ----------------------------------------------------------------------------

void wxListBox::Init()
{
    m_treeview = (GtkTreeView*) NULL;
#if wxUSE_CHECKLISTBOX
    m_hasCheckBoxes = false;
#endif // wxUSE_CHECKLISTBOX
}

bool wxListBox::Create( wxWindow *parent, wxWindowID id,
                        const wxPoint &pos, const wxSize &size,
                        const wxArrayString& choices,
                        long style, const wxValidator& validator,
                        const wxString &name )
{
    wxCArrayString chs(choices);

    return Create( parent, id, pos, size, chs.GetCount(), chs.GetStrings(),
                   style, validator, name );
}

bool wxListBox::Create( wxWindow *parent, wxWindowID id,
                        const wxPoint &pos, const wxSize &size,
                        int n, const wxString choices[],
                        long style, const wxValidator& validator,
                        const wxString &name )
{
    if (!PreCreation( parent, pos, size ) ||
        !CreateBase( parent, id, pos, size, style, validator, name ))
    {
        wxFAIL_MSG( wxT("wxListBox creation failed") );
        return false;
    }

    m_widget = gtk_scrolled_window_new( (GtkAdjustment*) NULL, (GtkAdjustment*) NULL );
    if (style & wxLB_ALWAYS_SB)
    {
      gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(m_widget),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
    }
    else
    {
      gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(m_widget),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
    }


    GtkScrolledWindowSetBorder(m_widget, style);

    m_treeview = GTK_TREE_VIEW( gtk_tree_view_new( ) );

    //wxListBox doesn't have a header :)
    //NB: If enabled SetFirstItem doesn't work correctly
    gtk_tree_view_set_headers_visible(m_treeview, FALSE);

#if wxUSE_CHECKLISTBOX
    if(m_hasCheckBoxes)
        ((wxCheckListBox*)this)->DoCreateCheckList();
#endif // wxUSE_CHECKLISTBOX

    // Create the data column
    gtk_tree_view_insert_column_with_attributes(m_treeview, -1, "",
                                                gtk_cell_renderer_text_new(),
                                                "text",
                                                WXLISTBOX_DATACOLUMN, NULL);

    // Now create+set the model (GtkListStore) - first argument # of columns
#if wxUSE_CHECKLISTBOX
    if(m_hasCheckBoxes)
        m_liststore = gtk_list_store_new(2, G_TYPE_BOOLEAN,
                                            GTK_TYPE_TREE_ENTRY);
    else
#endif
        m_liststore = gtk_list_store_new(1, GTK_TYPE_TREE_ENTRY);

    gtk_tree_view_set_model(m_treeview, GTK_TREE_MODEL(m_liststore));

    g_object_unref (m_liststore); //free on treeview destruction

    // Disable the pop-up textctrl that enables searching - note that
    // the docs specify that even if this disabled (which we are doing)
    // the user can still have it through the start-interactive-search
    // key binding...either way we want to provide a searchequal callback
    // NB: If this is enabled a doubleclick event (activate) gets sent
    //     on a successful search
    gtk_tree_view_set_search_column(m_treeview, WXLISTBOX_DATACOLUMN);
    gtk_tree_view_set_search_equal_func(m_treeview,
       (GtkTreeViewSearchEqualFunc) gtk_listbox_searchequal_callback,
                                        this,
                                        NULL);

    gtk_tree_view_set_enable_search(m_treeview, FALSE);

    GtkSelectionMode mode;
    if (style & wxLB_MULTIPLE)
    {
        mode = GTK_SELECTION_MULTIPLE;
    }
    else if (style & wxLB_EXTENDED)
    {
        mode = GTK_SELECTION_EXTENDED;
    }
    else
    {
        // if style was 0 set single mode
        m_windowStyle |= wxLB_SINGLE;
        mode = GTK_SELECTION_SINGLE;
    }

    GtkTreeSelection* selection = gtk_tree_view_get_selection( m_treeview );
    gtk_tree_selection_set_mode( selection, mode );

    // Handle sortable stuff
    if(HasFlag(wxLB_SORT))
    {
        // Setup sorting in ascending (wx) order
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(m_liststore),
                                             WXLISTBOX_DATACOLUMN,
                                             GTK_SORT_ASCENDING);

        // Set the sort callback
        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(m_liststore),
                                        WXLISTBOX_DATACOLUMN,
                   (GtkTreeIterCompareFunc) gtk_listbox_sort_callback,
                                        this, //userdata
                                        NULL //"destroy notifier"
                                       );
    }


    gtk_container_add (GTK_CONTAINER (m_widget), GTK_WIDGET(m_treeview) );

    gtk_widget_show( GTK_WIDGET(m_treeview) );
    m_focusWidget = GTK_WIDGET(m_treeview);

    Append(n, choices); // insert initial items

    // generate dclick events
    g_signal_connect_after(m_treeview, "row-activated",
                     G_CALLBACK(gtk_listbox_row_activated_callback), this);

    // for intercepting dclick generation by <ENTER>
    g_signal_connect (m_treeview, "key_press_event",
                      G_CALLBACK (gtk_listbox_key_press_callback),
                           this);
    m_parent->DoAddChild( this );

    PostCreation(size);
    SetInitialSize(size); // need this too because this is a wxControlWithItems

    g_signal_connect_after (selection, "changed",
                            G_CALLBACK (gtk_listitem_changed_callback), this);
 
    return true;
}

wxListBox::~wxListBox()
{
    m_hasVMT = false;

    Clear();
}

void wxListBox::GtkDisableEvents()
{
    GtkTreeSelection* selection = gtk_tree_view_get_selection( m_treeview );

    g_signal_handlers_block_by_func(selection,
                                (gpointer) gtk_listitem_changed_callback, this);
}

void wxListBox::GtkEnableEvents()
{
    GtkTreeSelection* selection = gtk_tree_view_get_selection( m_treeview );

    g_signal_handlers_unblock_by_func(selection,
                                (gpointer) gtk_listitem_changed_callback, this);
                                
    GtkUpdateOldSelection();
}

// ----------------------------------------------------------------------------
// adding items
// ----------------------------------------------------------------------------

int wxListBox::DoInsertItems(const wxArrayStringsAdapter& items,
                             unsigned int pos,
                             void **clientData,
                             wxClientDataType type)
{
    wxCHECK_MSG( m_treeview != NULL, wxNOT_FOUND, wxT("invalid listbox") );

    InvalidateBestSize();

    GtkTreeIter* pIter = NULL; // append by default
    GtkTreeIter iter;
    if ( pos != GetCount() )
    {
        wxCHECK_MSG( GtkGetIteratorFor(pos, &iter), wxNOT_FOUND,
                     wxT("internal wxListBox error in insertion") );

        pIter = &iter;
    }

    const unsigned int numItems = items.GetCount();
    for ( unsigned int i = 0; i < numItems; ++i )
    {
        GtkTreeEntry* entry = gtk_tree_entry_new();
        gtk_tree_entry_set_label(entry, wxGTK_CONV(items[i]));
        gtk_tree_entry_set_destroy_func(entry,
                (GtkTreeEntryDestroy)gtk_tree_entry_destroy_cb,
                            this);

        GtkTreeIter itercur;
        gtk_list_store_insert_before(m_liststore, &itercur, pIter);

        GtkSetItem(itercur, entry);

        g_object_unref (entry);

        if (clientData)
            AssignNewItemClientData(GtkGetIndexFor(itercur), clientData, i, type);
    }

    return pos + numItems - 1;
}

// ----------------------------------------------------------------------------
// deleting items
// ----------------------------------------------------------------------------

void wxListBox::DoClear()
{
    wxCHECK_RET( m_treeview != NULL, wxT("invalid listbox") );

    InvalidateBestSize();

    gtk_list_store_clear( m_liststore ); /* well, THAT was easy :) */
}

void wxListBox::DoDeleteOneItem(unsigned int n)
{
    wxCHECK_RET( m_treeview != NULL, wxT("invalid listbox") );

    InvalidateBestSize();

    GtkTreeIter iter;
    wxCHECK_RET( GtkGetIteratorFor(n, &iter), wxT("wrong listbox index") );

    // this returns false if iter is invalid (e.g. deleting item at end) but
    // since we don't use iter, we ignore the return value
    gtk_list_store_remove(m_liststore, &iter);
}

// ----------------------------------------------------------------------------
// helper functions for working with iterators
// ----------------------------------------------------------------------------

bool wxListBox::GtkGetIteratorFor(unsigned pos, GtkTreeIter *iter) const
{
    if ( !gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(m_liststore),
                                        iter, NULL, pos) )
    {
        wxLogDebug(wxT("gtk_tree_model_iter_nth_child(%u) failed"), pos);
        return false;
    }

    return true;
}

int wxListBox::GtkGetIndexFor(GtkTreeIter& iter) const
{
    GtkTreePath *path =
        gtk_tree_model_get_path(GTK_TREE_MODEL(m_liststore), &iter);

    gint* pIntPath = gtk_tree_path_get_indices(path);

    wxCHECK_MSG( pIntPath, wxNOT_FOUND, _T("failed to get iterator path") );

    int idx = pIntPath[0];

    gtk_tree_path_free( path );

    return idx;
}

// get GtkTreeEntry from position (note: you need to g_unref it if valid)
GtkTreeEntry *wxListBox::GtkGetEntry(unsigned n) const
{
    GtkTreeIter iter;
    if ( !GtkGetIteratorFor(n, &iter) )
        return NULL;


    GtkTreeEntry* entry = NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(m_liststore), &iter,
                       WXLISTBOX_DATACOLUMN, &entry, -1);

    return entry;
}

void wxListBox::GtkSetItem(GtkTreeIter& iter, const GtkTreeEntry *entry)
{
#if wxUSE_CHECKLISTBOX
    if ( m_hasCheckBoxes )
    {
        gtk_list_store_set(m_liststore, &iter,
                           0, FALSE, // FALSE == not toggled
                           1, entry,
                           -1);
    }
    else
#endif // wxUSE_CHECKLISTBOX
    {
        gtk_list_store_set(m_liststore, &iter, 0, entry, -1);
    }
}

// ----------------------------------------------------------------------------
// client data
// ----------------------------------------------------------------------------

void* wxListBox::DoGetItemClientData(unsigned int n) const
{
    wxCHECK_MSG( IsValid(n), NULL,
                 wxT("Invalid index passed to GetItemClientData") );

    GtkTreeEntry* entry = GtkGetEntry(n);
    wxCHECK_MSG(entry, NULL, wxT("could not get entry"));

    void* userdata = gtk_tree_entry_get_userdata( entry );
    g_object_unref (entry);
    return userdata;
}

void wxListBox::DoSetItemClientData(unsigned int n, void* clientData)
{
    wxCHECK_RET( IsValid(n),
                 wxT("Invalid index passed to SetItemClientData") );

    GtkTreeEntry* entry = GtkGetEntry(n);
    wxCHECK_RET(entry, wxT("could not get entry"));

    gtk_tree_entry_set_userdata( entry, clientData );
    g_object_unref (entry);
}

// ----------------------------------------------------------------------------
// string list access
// ----------------------------------------------------------------------------

void wxListBox::SetString(unsigned int n, const wxString& label)
{
    wxCHECK_RET( IsValid(n), wxT("invalid index in wxListBox::SetString") );
    wxCHECK_RET( m_treeview != NULL, wxT("invalid listbox") );

    GtkTreeEntry* entry = GtkGetEntry(n);
    wxCHECK_RET( entry, wxT("wrong listbox index") );

    // update the item itself
    gtk_tree_entry_set_label(entry, wxGTK_CONV(label));

    // and update the model which will refresh the tree too
    GtkTreeIter iter;
    wxCHECK_RET( GtkGetIteratorFor(n, &iter), _T("failed to get iterator") );

    // FIXME: this resets the checked status of a wxCheckListBox item

    GtkSetItem(iter, entry);
}

wxString wxListBox::GetString(unsigned int n) const
{
    wxCHECK_MSG( m_treeview != NULL, wxEmptyString, wxT("invalid listbox") );

    GtkTreeEntry* entry = GtkGetEntry(n);
    wxCHECK_MSG( entry, wxEmptyString, wxT("wrong listbox index") );

    wxString label = wxGTK_CONV_BACK( gtk_tree_entry_get_label(entry) );

    g_object_unref (entry);
    return label;
}

unsigned int wxListBox::GetCount() const
{
    wxCHECK_MSG( m_treeview != NULL, 0, wxT("invalid listbox") );

    return (unsigned int)gtk_tree_model_iter_n_children(GTK_TREE_MODEL(m_liststore), NULL);
}

int wxListBox::FindString( const wxString &item, bool bCase ) const
{
    wxCHECK_MSG( m_treeview != NULL, wxNOT_FOUND, wxT("invalid listbox") );

    //Sort of hackish - maybe there is a faster way
    unsigned int nCount = wxListBox::GetCount();

    for(unsigned int i = 0; i < nCount; ++i)
    {
        if( item.IsSameAs( wxListBox::GetString(i), bCase ) )
            return (int)i;
    }


    // it's not an error if the string is not found -> no wxCHECK
    return wxNOT_FOUND;
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------

int wxListBox::GetSelection() const
{
    wxCHECK_MSG( m_treeview != NULL, wxNOT_FOUND, wxT("invalid listbox"));
    wxCHECK_MSG( HasFlag(wxLB_SINGLE), wxNOT_FOUND,
                    wxT("must be single selection listbox"));

    GtkTreeIter iter;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(m_treeview);

    // only works on single-sel
    if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
        return wxNOT_FOUND;

    return GtkGetIndexFor(iter);
}

int wxListBox::GetSelections( wxArrayInt& aSelections ) const
{
    wxCHECK_MSG( m_treeview != NULL, wxNOT_FOUND, wxT("invalid listbox") );

    aSelections.Empty();

        int i = 0;
    GtkTreeIter iter;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(m_treeview);

    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_liststore), &iter))
    { //gtk_tree_selection_get_selected_rows is GTK 2.2+ so iter instead
        do
        {
            if (gtk_tree_selection_iter_is_selected(selection, &iter))
                aSelections.Add(i);

            i++;
        } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(m_liststore), &iter));
    }

    return aSelections.GetCount();
}

bool wxListBox::IsSelected( int n ) const
{
    wxCHECK_MSG( m_treeview != NULL, false, wxT("invalid listbox") );

    GtkTreeSelection* selection = gtk_tree_view_get_selection(m_treeview);

    GtkTreeIter iter;
    wxCHECK_MSG( GtkGetIteratorFor(n, &iter), false, wxT("Invalid index") );

    return gtk_tree_selection_iter_is_selected(selection, &iter);
}

void wxListBox::DoSetSelection( int n, bool select )
{
    wxCHECK_RET( m_treeview != NULL, wxT("invalid listbox") );

    GtkDisableEvents();
    
    GtkTreeSelection* selection = gtk_tree_view_get_selection(m_treeview);

    // passing -1 to SetSelection() is documented to deselect all items
    if ( n == wxNOT_FOUND )
    {
        gtk_tree_selection_unselect_all(selection);
        GtkEnableEvents();
        return;
    }

    wxCHECK_RET( IsValid(n), wxT("invalid index in wxListBox::SetSelection") );

    
    GtkTreeIter iter;
    wxCHECK_RET( GtkGetIteratorFor(n, &iter), wxT("Invalid index") );

    if (select)
        gtk_tree_selection_select_iter(selection, &iter);
    else
        gtk_tree_selection_unselect_iter(selection, &iter);

    GtkTreePath* path = gtk_tree_model_get_path(
                        GTK_TREE_MODEL(m_liststore), &iter);

    gtk_tree_view_scroll_to_cell(m_treeview, path, NULL, FALSE, 0.0f, 0.0f);

    gtk_tree_path_free(path);

    GtkEnableEvents();
}

void wxListBox::GtkUpdateOldSelection()
{
    if (HasFlag(wxLB_MULTIPLE) || HasFlag(wxLB_EXTENDED))
        GetSelections( m_oldSelection );
}

void wxListBox::DoScrollToCell(int n, float alignY, float alignX)
{
    wxCHECK_RET( m_treeview, wxT("invalid listbox") );
    wxCHECK_RET( IsValid(n), wxT("invalid index"));

    //RN: I have no idea why this line is needed...
    if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (m_treeview))
        return;

    GtkTreeIter iter;
    if ( !GtkGetIteratorFor(n, &iter) )
        return;

    GtkTreePath* path = gtk_tree_model_get_path(
                            GTK_TREE_MODEL(m_liststore), &iter);

    // Scroll to the desired cell (0.0 == topleft alignment)
    gtk_tree_view_scroll_to_cell(m_treeview, path, NULL,
                                 TRUE, alignY, alignX);

    gtk_tree_path_free(path);
}

void wxListBox::DoSetFirstItem(int n)
{
    DoScrollToCell(n, 0, 0);
}

void wxListBox::EnsureVisible(int n)
{
    DoScrollToCell(n, 0.5, 0);
}

// ----------------------------------------------------------------------------
// hittest
// ----------------------------------------------------------------------------

int wxListBox::DoListHitTest(const wxPoint& point) const
{
    // gtk_tree_view_get_path_at_pos() also gets items that are not visible and
    // we only want visible items we need to check for it manually here
    if ( !GetClientRect().Contains(point) )
        return wxNOT_FOUND;

    // need to translate from master window since it is in client coords
    gint binx, biny;
    gdk_window_get_geometry(gtk_tree_view_get_bin_window(m_treeview),
                            &binx, &biny, NULL, NULL, NULL);

    GtkTreePath* path;
    if ( !gtk_tree_view_get_path_at_pos
          (
            m_treeview,
            point.x - binx,
            point.y - biny,
            &path,
            NULL,   // [out] column (always 0 here)
            NULL,   // [out] x-coord relative to the cell (not interested)
            NULL    // [out] y-coord relative to the cell
          ) )
    {
        return wxNOT_FOUND;
    }

    int index = gtk_tree_path_get_indices(path)[0];
    gtk_tree_path_free(path);

    return index;
}

// ----------------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------------

#if wxUSE_TOOLTIPS
void wxListBox::ApplyToolTip( GtkTooltips *tips, const gchar *tip )
{
    // RN: Is this needed anymore?
    gtk_tooltips_set_tip( tips, GTK_WIDGET( m_treeview ), tip, (gchar*) NULL );
}
#endif // wxUSE_TOOLTIPS

GtkWidget *wxListBox::GetConnectWidget()
{
    // the correct widget for listbox events (such as mouse clicks for example)
    // is m_treeview, not the parent scrolled window
    return GTK_WIDGET(m_treeview);
}

GdkWindow *wxListBox::GTKGetWindow(wxArrayGdkWindows& WXUNUSED(windows)) const
{
    return gtk_tree_view_get_bin_window(m_treeview);
}

void wxListBox::DoApplyWidgetStyle(GtkRcStyle *style)
{
    if (m_hasBgCol && m_backgroundColour.Ok())
    {
        GdkWindow *window = gtk_tree_view_get_bin_window(m_treeview);
        if (window)
        {
            m_backgroundColour.CalcPixel( gdk_drawable_get_colormap( window ) );
            gdk_window_set_background( window, m_backgroundColour.GetColor() );
            gdk_window_clear( window );
        }
    }

    gtk_widget_modify_style( GTK_WIDGET(m_treeview), style );
}

wxSize wxListBox::DoGetBestSize() const
{
    wxCHECK_MSG(m_treeview, wxDefaultSize, wxT("invalid tree view"));

    // Start with a minimum size that's not too small
    int cx, cy;
    GetTextExtent( wxT("X"), &cx, &cy);
    int lbWidth = 0;
    int lbHeight = 10;

    // Find the widest string.
    const unsigned int count = GetCount();
    if ( count )
    {
        int wLine;
        for ( unsigned int i = 0; i < count; i++ )
        {
            GetTextExtent(GetString(i), &wLine, NULL);
            if ( wLine > lbWidth )
                lbWidth = wLine;
        }
    }

    lbWidth += 3 * cx;

    // And just a bit more for the checkbox if present and then some
    // (these are rough guesses)
#if wxUSE_CHECKLISTBOX
    if ( m_hasCheckBoxes )
    {
        lbWidth += 35;
        cy = cy > 25 ? cy : 25; // rough height of checkbox
    }
#endif

    // Add room for the scrollbar
    lbWidth += wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);

    // Don't make the listbox too tall but don't make it too small neither
    lbHeight = (cy+4) * wxMin(wxMax(count, 3), 10);

    wxSize best(lbWidth, lbHeight);
    CacheBestSize(best);
    return best;
}

// static
wxVisualAttributes
wxListBox::GetClassDefaultAttributes(wxWindowVariant WXUNUSED(variant))
{
    return GetDefaultAttributesFromGTKWidget(gtk_tree_view_new, true);
}

#endif // wxUSE_LISTBOX

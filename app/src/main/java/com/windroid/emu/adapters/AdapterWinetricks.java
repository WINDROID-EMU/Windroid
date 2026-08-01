package com.windroid.emu.adapters;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;

import com.windroid.emu.R;
import com.windroid.emu.core.WinetricksItem;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

public class AdapterWinetricks extends RecyclerView.Adapter<AdapterWinetricks.ViewHolder> {

    private final List<WinetricksItem> fullList;
    private List<WinetricksItem> filteredList;

    public AdapterWinetricks(List<WinetricksItem> list) {
        this.fullList = list;
        this.filteredList = new ArrayList<>(list);
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.adapter_winetricks_item, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        WinetricksItem item = filteredList.get(position);
        holder.iconView.setImageResource(item.getIconResId());
        holder.nameText.setText(item.getSimpleName());
        holder.checkBox.setChecked(item.isSelected());
        
        // Configura o estado visual baseado em instalado ou não
        if (item.isInstalled()) {
            holder.itemView.setAlpha(0.6f);
            holder.checkBox.setEnabled(false);
            holder.checkBox.setChecked(true);
        } else {
            holder.itemView.setAlpha(1.0f);
            holder.checkBox.setEnabled(true);
            holder.checkBox.setChecked(item.isSelected());
        }
        
        // Click listener no itemView inteiro
        holder.itemView.setOnClickListener(v -> {
            if (item.isInstalled()) return;
            boolean newState = !item.isSelected();
            item.setSelected(newState);
            holder.checkBox.setChecked(newState);
        });
        
        // Também permite clicar diretamente no checkbox
        holder.checkBox.setOnClickListener(v -> {
            if (item.isInstalled()) {
                holder.checkBox.setChecked(true);
                return;
            }
            boolean newState = holder.checkBox.isChecked();
            item.setSelected(newState);
        });
    }

    @Override
    public int getItemCount() {
        return filteredList.size();
    }

    public void filter(String query) {
        if (query == null || query.isEmpty()) {
            filteredList = new ArrayList<>(fullList);
        } else {
            String lowerQuery = query.toLowerCase();
            filteredList = fullList.stream()
                    .filter(item -> item.getName().toLowerCase().contains(lowerQuery) ||
                            item.getSimpleName().toLowerCase().contains(lowerQuery) ||
                            item.getDescription().toLowerCase().contains(lowerQuery) ||
                            item.getCategory().toLowerCase().contains(lowerQuery))
                    .collect(Collectors.toList());
        }
        notifyDataSetChanged();
    }

    public void updateList() {
        filter("");
    }

    public List<WinetricksItem> getSelectedItems() {
        return fullList.stream().filter(WinetricksItem::isSelected).collect(Collectors.toList());
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        ImageView iconView;
        TextView nameText;
        CheckBox checkBox;

        public ViewHolder(@NonNull View itemView) {
            super(itemView);
            iconView = itemView.findViewById(R.id.winetricksItemIcon);
            nameText = itemView.findViewById(R.id.winetricksItemName);
            checkBox = itemView.findViewById(R.id.winetricksItemCheckBox);
        }
    }
}

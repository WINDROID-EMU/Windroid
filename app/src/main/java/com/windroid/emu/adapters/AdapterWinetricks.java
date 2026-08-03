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

        if (item.isInstalled()) {
            // Pacote já instalado: mostra a caixinha verde com o sinal de V confirmando,
            // esconde o checkbox de seleção e bloqueia totalmente o clique no item.
            holder.itemView.setAlpha(0.6f);
            holder.checkBox.setVisibility(View.GONE);
            holder.installedIcon.setVisibility(View.VISIBLE);

            holder.itemView.setClickable(false);
            holder.itemView.setOnClickListener(null);
            holder.checkBox.setOnClickListener(null);
        } else {
            // Pacote ainda não instalado/validado: continua acessível e selecionável normalmente.
            holder.itemView.setAlpha(1.0f);
            holder.checkBox.setVisibility(View.VISIBLE);
            holder.installedIcon.setVisibility(View.GONE);
            holder.checkBox.setEnabled(true);
            holder.checkBox.setChecked(item.isSelected());

            holder.itemView.setClickable(true);
            holder.itemView.setOnClickListener(v -> {
                boolean newState = !item.isSelected();
                item.setSelected(newState);
                holder.checkBox.setChecked(newState);
            });

            holder.checkBox.setOnClickListener(v -> {
                boolean newState = holder.checkBox.isChecked();
                item.setSelected(newState);
            });
        }
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
        ImageView installedIcon;

        public ViewHolder(@NonNull View itemView) {
            super(itemView);
            iconView = itemView.findViewById(R.id.winetricksItemIcon);
            nameText = itemView.findViewById(R.id.winetricksItemName);
            checkBox = itemView.findViewById(R.id.winetricksItemCheckBox);
            installedIcon = itemView.findViewById(R.id.winetricksItemInstalledIcon);
        }
    }
}
